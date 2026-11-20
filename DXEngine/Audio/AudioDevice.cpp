
#include "Precomp.h"
#include "AudioDevice.h"
#include "AudioSource.h"
#include "GalaxyMixer.h"
#include "Engine.h"
#include "Native/NObject.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Packages/Engine/Resources/USound.h"
#include <mutex>
#include "Utils/Exception.h"
#include <atomic>
#include <map>
#include <cmath>
#include <queue>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

class OpenALAudioDevice : public AudioDevice
{
public:
	OpenALAudioDevice(int inFrequency, int numVoices, int mixAheadMs, bool inStereo)
	{
		frequency = inFrequency;
		mixBufferCount = std::clamp(mixAheadMs / 10, 2, 25);
		stereo = inStereo;

		// Init OpenAL, on the system's default device
		alDevice = alcOpenDevice(NULL);
		if (alDevice == nullptr)
			Exception::Throw("Failed to initialize OpenAL device");

		// The context at the mixing rate chosen (the launcher's), so OpenAL
		// mixes the source at it rather than resampling a mix made at it.
		// No output limiter: the music and the sounds come saturated as
		// Galaxy saturates them (MixMusic), where OpenAL Soft's limiter, on
		// for an integer output, turned their sum down instead (bugs.md
		// AUD.16)
		const ALCint ctxAttribs[] =
		{
			ALC_FREQUENCY, inFrequency,
			ALC_REFRESH, 60,
			ALC_SYNC, ALC_FALSE,
			ALC_OUTPUT_LIMITER_SOFT, ALC_FALSE,
			0
		};
		static_assert(sizeof(ctxAttribs) == sizeof(contextAttribs));
		std::copy(std::begin(ctxAttribs), std::end(ctxAttribs), contextAttribs);

		// The device closed again before a failure is thrown: the engine
		// plays on without it (UDXEngineAudioDevice::InitDevice)
		alContext = alcCreateContext(alDevice, contextAttribs);
		if (alContext == nullptr)
		{
			alcCloseDevice(alDevice);
			Exception::Throw("Failed to initialize OpenAL context");
		}

		if (alcMakeContextCurrent(alContext) == ALC_FALSE)
		{
			alcDestroyContext(alContext);
			alcCloseDevice(alDevice);
			Exception::Throw("Failed to make OpenAL context current");
		}
		canTellConnected = alcIsExtensionPresent(alDevice, "ALC_EXT_disconnect") == ALC_TRUE;
		WatchDefaultDevice();

		// Deus Ex's sounds are mixed as Galaxy mixes them, in Galaxy's 32
		// channel records, and the music added to them, the output streamed
		// through one source at OutputRate, as Galaxy's goes out, so its
		// reverb's delays are the original's samples; nothing is spatialized
		// by OpenAL.
		mixer = std::make_unique<GalaxyMixer>(frequency, 32, stereo);
		StartMixStream();
	}

	~OpenALAudioDevice()
	{
		StopMixStream();
		if (eventCallback)
			eventCallback(nullptr, nullptr);

		alcDestroyContext(alContext);
		alcCloseDevice(alDevice);
	}

	int GetTotalChannels() override
	{
		return mixer->GetChannels();
	}

	void AddSound(USound* sound) override
	{
		// The mixer's own 16-bit copy, which a channel keeps while it plays,
		// two silent frames past the end for the last frame's interpolation.
		// An MP3, or a stereo sound of 16-bit samples, plays 4,096 frames of
		// silence first: Galaxy decodes it, or splits its sides, into a
		// zeroed ring of 64 KB for each channel, its first fill 8 KB past the
		// voice's place (galaxy-dll.md, Playing a sound; its refill,
		// Galaxy.dll 0x1060e340), and the mixer plays it on from there as
		// that ring's fill sets its end (GalaxyMixer::FillRing). An 8-bit
		// stereo one's ring turns at half its length there, and plays from
		// its start here, by choice (docs/NATIVES.md, sound).
		auto sample = std::make_shared<GalaxySample>();
		sample->channels = std::max(sound->channels, 1);
		sample->ringed = sound->Format == "mp3" || sound->Format == "mp2" || (sample->channels == 2 && !sound->eightBit);
		size_t lead = sample->ringed ? 4096 : 0;
		sample->frames = lead + sound->samples.size() / sample->channels;
		sample->rate = sound->frequency;
		sample->data.resize((sample->frames + 2) * sample->channels);
		int16_t* frames = sample->data.data() + lead * sample->channels;
		for (size_t i = 0, count = (sample->frames - lead) * sample->channels; i < count; i++)
			frames[i] = (int16_t)std::clamp((int)std::lround(sound->samples[i] * 32768.0f), -32768, 32767);
		sample->looped = sound->loopInfo.Looped;
		sample->loopStart = lead + sound->loopInfo.LoopStart;
		sample->loopEnd = lead + sound->loopInfo.LoopEnd;
		RemoveSound(sound);
		HeldSound& held = mixedSounds[sound];
		held.bytes = sample->data.size() * sizeof(int16_t);
		held.mp3 = sound->Format == "mp3" || sound->Format == "mp2";
		held.lastUse = ++useClock;
		held.sample = std::move(sample);
		if (held.mp3)
			heldMp3Bytes += held.bytes;
	}

	void RemoveSound(USound* sound) override
	{
		auto it = mixedSounds.find(sound);
		if (it == mixedSounds.end())
			return;
		if (it->second.mp3)
			heldMp3Bytes -= it->second.bytes;
		mixedSounds.erase(it);
	}

	bool HasSound(USound* sound) override
	{
		return mixedSounds.find(sound) != mixedSounds.end();
	}

	// A guard of ours (bugs.md AUD.26): Galaxy decodes an MP3 into a 64 KB
	// ring for each voice that plays it, as it plays, and keeps only its
	// file's bytes (galaxy-dll.md, Playing a sound), where the mixer's copy
	// is the sound decoded whole, some 94 KB a second of the game's speech.
	// So past MaxMp3Bytes of MP3s held, those played longest ago go until it
	// is under again, never one a channel plays (the channel's reference) nor
	// one played or added since the last update; the next play decodes it
	// again, as its first did, to the same samples. A WAV's copy stays, as
	// Galaxy keeps a WAV's data whole.
	void ReleaseOldSounds()
	{
		while (heldMp3Bytes > MaxMp3Bytes)
		{
			auto oldest = mixedSounds.end();
			for (auto it = mixedSounds.begin(); it != mixedSounds.end(); ++it)
			{
				const HeldSound& held = it->second;
				if (held.mp3 && held.lastUse <= lastUpdateUse && held.sample.use_count() == 1 &&
					(oldest == mixedSounds.end() || held.lastUse < oldest->second.lastUse))
					oldest = it;
			}
			if (oldest == mixedSounds.end())
				break;
			heldMp3Bytes -= oldest->second.bytes;
			mixedSounds.erase(oldest);
		}
		lastUpdateUse = useClock;
	}

	bool IsPlaying(int channel) override
	{
		return mixer->IsPlaying(channel);
	}

	void PlayMusic(std::unique_ptr<AudioSource> source, float volume) override
	{
		int sections = source ? source->GetSections() : -1;
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.sections = sections;
		musicThreadData.music = std::move(source);
		musicThreadData.musicUpdate = true;
		musicThreadData.volume = volume;
		musicThreadData.generation++;
		musicThreadData.currentOrder.store(-1);
	}

	int GetMusicOrder() override
	{
		return musicThreadData.currentOrder.load();
	}

	void SetReverb(const ReverbSettings* settings) override
	{
		mixer->SetReverb(settings);
	}

	// A jump's order count is Galaxy's at once, as its Update writes it back
	// from the next frame on: the main thread reads it that frame, not the
	// pre-jump order until the mix thread's next block. For a song Galaxy
	// plays, glxControlMusic leaves it the order before the section, 255
	// before order 0, the section starting as the playing row ends; a section
	// past the last order it refuses, the count as it was and the song
	// playing on, at the change's gain (Galaxy.dll 0x1060d0e0; bugs.md
	// AUD.17). Another source jumps to the order asked.
	bool SetMusicOrder(int order, float volume, bool atOnce = false) override
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.volume = volume;
		int sections = musicThreadData.sections;
		if (order < 0 || (sections >= 0 && order >= sections))
			return false;
		musicThreadData.jumpOrder = order;
		musicThreadData.jumpAtOnce = atOnce;
		musicThreadData.currentOrder.store(sections >= 0 ? (order + 255) % 256 : order);
		return true;
	}

	void PlayMixedSound(int channel, USound* sound, int volume, int panning, float pitch) override
	{
		if (!std::isfinite(pitch))
			pitch = 1.0f;
		if (channel >= mixer->GetChannels())
			Exception::Throw("Invalid PlayMixedSound arguments");

		auto it = mixedSounds.find(sound);
		if (it != mixedSounds.end())
			it->second.lastUse = ++useClock;
		mixer->Play(channel, it != mixedSounds.end() ? it->second.sample : nullptr, volume, panning, pitch);
	}

	void UpdateMixedSound(int channel, int volume, int panning, float pitch) override
	{
		if (!std::isfinite(pitch))
			pitch = 1.0f;
		if (channel >= mixer->GetChannels())
			Exception::Throw("Invalid UpdateMixedSound arguments");

		mixer->Update(channel, volume, panning, pitch);
	}

	void StopSound(int channel) override
	{
		if (channel >= mixer->GetChannels())
			Exception::Throw("Invalid StopSound arguments");
		mixer->Stop(channel);
	}

	bool GetMixedSoundPosition(int channel, uint64_t& frame) override
	{
		return channel < mixer->GetChannels() && mixer->GetPosition(channel, frame);
	}

	bool ReadMixedSound(int channel, uint64_t frame, int16_t* frames, int count) override
	{
		return channel < mixer->GetChannels() && mixer->Read(channel, frame, frames, count);
	}

	// Applied by the mix thread, after any change of song or order handed
	// over before it (MixMusic)
	void SetMusicVolume(float volume) override
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.volume = volume;
	}

	void SetSoundVolume(float volume) override
	{
		globalSoundVolume = volume;
		mixer->SetMasterVolume(std::max(globalSoundVolume, globalSpeechVolume));
	}

	void SetSpeechVolume(float volume) override
	{
		globalSpeechVolume = volume;
		mixer->SetMasterVolume(std::max(globalSoundVolume, globalSpeechVolume));
	}

	void Update() override
	{
		LogDeviceNotes();
		std::string failure;
		{
			std::unique_lock lock(musicThreadData.mutex);
			failure.swap(musicThreadData.failure);
		}
		if (!failure.empty())
			LogMessage("Music: the song stopped: " + failure);
		ReleaseOldSounds();
	}

	// The song's next frames at the mix rate, stereo: a source at another
	// rate read between its frames linearly, from its first (a place of 2
	// reads it and the next), a mono one in both channels, one of more than
	// two its front pair (GetRightChannel). What a source that ends or reads
	// short leaves is silence.
	void ReadMusic(AudioSource* music, float* out, int frames)
	{
		int channels = std::max(music->GetChannels(), 1);
		int right = channels > 1 ? std::clamp(music->GetRightChannel(), 1, channels - 1) : 0;
		int rate = music->GetFrequency();
		if (rate == frequency && channels == 2)
		{
			size_t got = music->ReadSamples(out, (size_t)frames * 2);
			std::fill(out + std::min(got, (size_t)frames * 2), out + (size_t)frames * 2, 0.0f);
			return;
		}
		double step = rate > 0 ? (double)rate / frequency : 1.0;
		for (int i = 0; i < frames; i++)
		{
			while (musicPlace >= 1.0)
			{
				musicLast[0] = musicNext[0];
				musicLast[1] = musicNext[1];
				musicFrame.resize(channels);
				if (music->ReadSamples(musicFrame.data(), channels) < (size_t)channels)
					std::fill(musicFrame.begin(), musicFrame.end(), 0.0f);
				musicNext[0] = musicFrame[0];
				musicNext[1] = musicFrame[right];
				musicPlace -= 1.0;
			}
			float t = (float)musicPlace;
			out[2 * i] = musicLast[0] + (musicNext[0] - musicLast[0]) * t;
			out[2 * i + 1] = musicLast[1] + (musicNext[1] - musicLast[1]) * t;
			musicPlace += step;
		}
	}

	// The music added to the sounds' mix as Galaxy adds them (Galaxy.dll
	// 0x106146a0, 0x10623eed): the song's frames at its gain saturated at 16
	// bits, as Galaxy writes its music out, then the sum, so the output is
	// saturated before any volume of the system's, whatever its format
	// (bugs.md AUD.16, AUD.22, AUD.23). On the mix thread, which renders the
	// song as it mixes, the music heard as it changes, by choice (Galaxy's
	// comes a turn of its second-long buffer later): a new song or a jump
	// handed over with its gain, so no frame of the old one plays at the new
	// gain (bugs.md AUD.12). A song that fails stops, and the game goes on
	// without it: the failure is left for the main thread to log.
	void MixMusic(float* block, int frames)
	{
		std::unique_lock lock(musicThreadData.mutex);
		if (musicThreadData.musicUpdate)
		{
			currentMusic = std::move(musicThreadData.music);
			if (currentMusic)
			{
				currentMusic->SetMono(!stereo);
				currentMusic->SetFrequency(frequency);
			}
			musicThreadData.musicUpdate = false;
			mixer->SetMusicTempo(0.0);
			musicPlace = 2.0;
		}
		int jumpOrder = musicThreadData.jumpOrder;
		bool jumpAtOnce = musicThreadData.jumpAtOnce;
		int generation = musicThreadData.generation;
		float volume = musicThreadData.volume;
		musicThreadData.jumpOrder = -1;
		lock.unlock();

		if (currentMusic)
		{
			try
			{
				if (jumpOrder >= 0)
				{
					currentMusic->SetOrder(jumpOrder, jumpAtOnce);
					musicPlace = 2.0;
				}

				musicBlock.resize((size_t)frames * 2);
				ReadMusic(currentMusic.get(), musicBlock.data(), frames);
				// Mono without UseStereo, the music too, as Galaxy's whole
				// output is (bugs.md AUD.14): a song as its source reads it
				// mono (SetMono), any other's two channels at their mean
				if (!stereo && currentMusic->GetChannels() >= 2)
				{
					for (int i = 0; i < frames; i++)
						musicBlock[2 * i] = musicBlock[2 * i + 1] = 0.5f * (musicBlock[2 * i] + musicBlock[2 * i + 1]);
				}
				for (int i = 0; i < frames * 2; i++)
					block[i] += std::clamp(musicBlock[i] * volume, -1.0f, 32767.0f / 32768.0f);
				mixer->SetMusicTempo(currentMusic->GetTempo());

				// Not while a jump asked since this block read it waits: its
				// count, stored as it was asked, stands until the next block
				std::unique_lock orderLock(musicThreadData.mutex);
				if (musicThreadData.generation == generation && musicThreadData.jumpOrder < 0)
					musicThreadData.currentOrder.store(currentMusic->GetOrder());
			}
			catch (const std::exception& e)
			{
				currentMusic.reset();
				mixer->SetMusicTempo(0.0);
				musicThreadData.currentOrder.store(-1);
				std::unique_lock failedLock(musicThreadData.mutex);
				musicThreadData.failure = e.what();
			}
		}

		for (int i = 0; i < frames * 2; i++)
			block[i] = std::clamp(block[i], -1.0f, 32767.0f / 32768.0f);
	}

	// The output: one stereo source, straight to the speakers, as Galaxy
	// adds its mix to its output as it is, where without direct channels
	// OpenAL's HRTF (its own choice on headphones), UHJ or a surround layout
	// made the two channels two virtual speakers at 30 degrees (bugs.md
	// AUD.13); its queue 10 ms buffers deep to Latency, Galaxy's mix-ahead
	// (40 ms by default); mono without UseStereo, as the mixer folds it.
	void StartMixStream()
	{
		mixBufferFrames = std::max(frequency / 100, 64);
		CreateMixSource();
		mixThread = std::thread([this]() { MixThreadMain(); });
	}

	void StopMixStream()
	{
		mixExit = true;
		mixThread.join();
		DeleteMixSource();
	}

	void CreateMixSource()
	{
		alGenSources(1, &alMixSource);
		alSourcei(alMixSource, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);
		alSourcei(alMixSource, AL_SOURCE_RELATIVE, AL_TRUE);
		alSource3f(alMixSource, AL_POSITION, 0.0f, 0.0f, 0.0f);
		alSourcef(alMixSource, AL_ROLLOFF_FACTOR, 0.0f);
		if (alIsExtensionPresent("AL_SOFT_direct_channels"))
			alSourcei(alMixSource, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
		alMixBuffers.resize(mixBufferCount);
		alGenBuffers((ALsizei)alMixBuffers.size(), &alMixBuffers[0]);
		alGetError();
	}

	void DeleteMixSource()
	{
		alSourceStop(alMixSource);
		alSourcei(alMixSource, AL_BUFFER, 0);
		alDeleteSources(1, &alMixSource);
		alDeleteBuffers((ALsizei)alMixBuffers.size(), &alMixBuffers[0]);
	}

	// False once OpenAL Soft finds the device gone (ALC_EXT_disconnect: a
	// headset unplugged on ALSA, a sink removed under a PulseAudio stream
	// that may not move, a WASAPI endpoint removed or changed)
	bool DeviceConnected()
	{
		if (!canTellConnected)
			return true;
		ALCint connected = ALC_TRUE;
		alcGetIntegerv(alDevice, ALC_CONNECTED, 1, &connected);
		return connected != ALC_FALSE;
	}

	// The system's default device opened in place of the lost one, with the
	// first one's context attributes, and the output's source made again on
	// it; false, the lost one kept, when none opens. A full open, not
	// ALC_SOFT_reopen_device, which the systems' OpenAL Soft before 1.23
	// lacks.
	bool ReopenDevice()
	{
		ALCdevice* device = alcOpenDevice(nullptr);
		if (device == nullptr)
			return false;
		ALCcontext* context = alcCreateContext(device, contextAttribs);
		if (context == nullptr)
		{
			alcCloseDevice(device);
			return false;
		}
		DeleteMixSource();
		if (alcMakeContextCurrent(context) == ALC_FALSE)
		{
			alcDestroyContext(context);
			alcCloseDevice(device);
			CreateMixSource();
			return false;
		}
		alcDestroyContext(alContext);
		alcCloseDevice(alDevice);
		alDevice = device;
		alContext = context;
		canTellConnected = alcIsExtensionPresent(alDevice, "ALC_EXT_disconnect") == ALC_TRUE;
		CreateMixSource();
		return true;
	}

	// OpenAL Soft's word that the system's default output is another one
	// (ALC_SOFT_system_events, its 1.24 and later; asked for by name, as a
	// system's own library may be an older one): on Windows the output is
	// opened again on it, as the original's DirectSound follows the default
	// device there (bugs.md AUD.27); elsewhere the sound server moves the
	// stream itself, and may keep it where the player put it, so nothing is
	// asked. The call comes on a thread of OpenAL Soft's and only raises
	// the mark.
	static void ALC_APIENTRY OnSystemEvent(ALCenum eventType, ALCenum deviceType, ALCdevice*, ALCsizei, const ALCchar*, void* userParam) noexcept
	{
		if (eventType == ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT && deviceType == ALC_PLAYBACK_DEVICE_SOFT)
			static_cast<OpenALAudioDevice*>(userParam)->defaultChanged = true;
	}

	void WatchDefaultDevice()
	{
#ifdef WIN32
		if (alcIsExtensionPresent(nullptr, "ALC_SOFT_system_events") != ALC_TRUE)
			return;
		auto control = (LPALCEVENTCONTROLSOFT)alcGetProcAddress(nullptr, "alcEventControlSOFT");
		auto callback = (LPALCEVENTCALLBACKSOFT)alcGetProcAddress(nullptr, "alcEventCallbackSOFT");
		if (!control || !callback)
			return;
		callback(&OpenALAudioDevice::OnSystemEvent, this);
		const ALCenum events[] = { ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT };
		if (control(1, events, ALC_TRUE) == ALC_TRUE)
			eventCallback = callback;
		else
			callback(nullptr, nullptr);
#endif
	}

	// Whether the device the output is on is the system's default one, by
	// their full names; true where they cannot be told (no reopening then)
	bool OnDefaultDevice()
	{
		if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT") != ALC_TRUE)
			return true;
		const ALCchar* now = alcGetString(alDevice, ALC_ALL_DEVICES_SPECIFIER);
		const ALCchar* def = alcGetString(nullptr, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
		return !now || !def || std::string(now) == def;
	}

	// A line for the main thread to log (Update)
	void TellMainThread(std::string note)
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.deviceNotes.push_back(std::move(note));
	}

	void LogDeviceNotes()
	{
		std::vector<std::string> notes;
		{
			std::unique_lock lock(musicThreadData.mutex);
			notes.swap(musicThreadData.deviceNotes);
		}
		for (const std::string& note : notes)
			LogMessage(note);
	}

	void MixThreadMain()
	{
		std::vector<float> block(mixBufferFrames * 2);
		auto mix = [&]() {
			mixer->Mix(block.data(), mixBufferFrames);
			MixMusic(block.data(), mixBufferFrames);
		};
		auto queue = [&](ALuint buffer) {
			mix();
			alBufferData(buffer, AL_FORMAT_STEREO_FLOAT32, block.data(), (ALsizei)(block.size() * sizeof(float)), frequency);
			alSourceQueueBuffers(alMixSource, 1, &buffer);
		};
		auto start = [&]() {
			for (ALuint buffer : alMixBuffers)
				queue(buffer);
			alSourcePlay(alMixSource);
		};

		start();

		// A lost device's source stops and reports every buffer it holds
		// played, which were mixed and queued again at once, the sounds and
		// the song raced through. Instead, while it is gone, the mix goes on
		// at the clock's pace and is not heard, the sounds and the song
		// playing on, at most a queue's worth at once (after a suspend); the
		// default device is opened again each second until one opens, and
		// the output starts on it as at the start. A platform matter, which
		// the original's DirectSound leaves to Windows (bugs.md AUD.25).
		using clock = std::chrono::steady_clock;
		bool lost = false, followPending = false;
		clock::time_point lostAt, nextOpen, followAt;
		int64_t lostBlocks = 0;
		while (!mixExit)
		{
			using namespace std::chrono_literals;
			if (!lost && !DeviceConnected())
			{
				lost = true;
				lostAt = nextOpen = clock::now();
				lostBlocks = 0;
				TellMainThread("Audio: the output device is gone: the game plays on unheard until a default device opens");
			}
			// A new default output, followed once its events have been quiet a
			// quarter second (Windows tells of each role's change): the
			// output opened on it and started as at the start; one that will
			// not open leaves the output where it is
			if (!lost && defaultChanged.exchange(false))
			{
				followPending = true;
				followAt = clock::now() + 250ms;
			}
			if (!lost && followPending && clock::now() >= followAt)
			{
				followPending = false;
				if (!OnDefaultDevice() && ReopenDevice())
				{
					const ALCchar* name = alcGetString(alDevice, ALC_DEVICE_SPECIFIER);
					TellMainThread(std::string("Audio: the output follows the system's default device, ") + (name ? name : "unnamed"));
					start();
				}
			}
			if (lost)
			{
				auto now = clock::now();
				int64_t due = std::chrono::duration_cast<std::chrono::microseconds>(now - lostAt).count() * frequency / (1000000LL * mixBufferFrames);
				lostBlocks = std::max(lostBlocks, due - mixBufferCount);
				for (; lostBlocks < due; lostBlocks++)
					mix();
				if (now >= nextOpen)
				{
					nextOpen = now + 1s;
					if (ReopenDevice())
					{
						lost = false;
						const ALCchar* name = alcGetString(alDevice, ALC_DEVICE_SPECIFIER);
						TellMainThread(std::string("Audio: the output opened again on ") + (name ? name : "the default device"));
						start();
					}
				}
				std::this_thread::sleep_for(2ms);
				continue;
			}

			ALint processed = 0;
			alGetSourcei(alMixSource, AL_BUFFERS_PROCESSED, &processed);
			while (processed-- > 0)
			{
				ALuint buffer = 0;
				alSourceUnqueueBuffers(alMixSource, 1, &buffer);
				if (buffer == 0)
					break;
				queue(buffer);
			}

			// Played dry before a buffer came: on again.
			ALint state = AL_PLAYING;
			alGetSourcei(alMixSource, AL_SOURCE_STATE, &state);
			if (state != AL_PLAYING)
				alSourcePlay(alMixSource);

			std::this_thread::sleep_for(2ms);
		}
	}

	ALCdevice* alDevice = nullptr;
	ALCcontext* alContext = nullptr;
	ALCint contextAttribs[9] = {};      // the context's, for a device opened again
	bool canTellConnected = false;      // ALC_EXT_disconnect's ALC_CONNECTED answers
	LPALCEVENTCALLBACKSOFT eventCallback = nullptr;   // set while the default device's changes are asked for
	std::atomic<bool> defaultChanged{ false };        // raised by OpenAL Soft's thread, taken by the mixing one

	// Deus Ex's mixer, the sounds it holds, and its output
	std::unique_ptr<GalaxyMixer> mixer;
	struct HeldSound
	{
		std::shared_ptr<const GalaxySample> sample;
		size_t bytes = 0;
		bool mp3 = false;
		uint64_t lastUse = 0;   // useClock at its last play or its decoding
	};
	std::unordered_map<USound*, HeldSound> mixedSounds;
	// 32 MB, some six minutes of the game's speech, each line some 94 KB a
	// second decoded against its MP3's 6 KB
	static constexpr size_t MaxMp3Bytes = (size_t)32 << 20;
	size_t heldMp3Bytes = 0;
	uint64_t useClock = 0;
	uint64_t lastUpdateUse = 0;
	ALuint alMixSource = 0;
	Array<ALuint> alMixBuffers;
	int mixBufferFrames = 0;
	int mixBufferCount = 4;
	bool stereo = true;
	std::thread mixThread;
	std::atomic<bool> mixExit{ false };

	int frequency = 48000;

	// What the main thread hands the mix thread, under the mutex
	struct
	{
		std::mutex mutex;
		std::unique_ptr<AudioSource> music;
		bool musicUpdate = false;
		int jumpOrder = -1;                     // an order to jump the playing song to
		bool jumpAtOnce = false;                // from the row's next tick, not its end (MUSICORDER)
		int sections = -1;                      // the sections the song set takes, -1 any (AudioSource::GetSections)
		std::atomic<int> currentOrder{ -1 };    // the order playing: a jump's count at once, else the music thread's
		int generation = 0;                     // counts the songs set, so the thread's order is its own song's
		float volume = 0.0f;                    // the music's gain, for the mix thread to play it at
		std::string failure;                    // why the song stopped, for the main thread to log
		std::vector<std::string> deviceNotes;   // what became of the output, for it to log
	} musicThreadData;

	// The mix thread's song, and its place between a source's frames
	std::unique_ptr<AudioSource> currentMusic;
	std::vector<float> musicBlock;
	std::vector<float> musicFrame;          // one frame of a source read between its frames
	double musicPlace = 2.0;
	float musicLast[2] = {}, musicNext[2] = {};
	float globalSoundVolume = 1.0f;
	float globalSpeechVolume = 1.0f;
};

std::unique_ptr<AudioDevice> AudioDevice::Create(int frequency, int numVoices, int mixAheadMs, bool stereo)
{
	return std::make_unique<OpenALAudioDevice>(frequency, numVoices, mixAheadMs, stereo);
}

class OpenALAudioCapture : public AudioCapture
{
public:
	OpenALAudioCapture(ALCdevice* device) : device(device) {}
	~OpenALAudioCapture() override
	{
		alcCaptureStop(device);
		alcCaptureCloseDevice(device);
	}

	int Read(int16_t* samples, int count) override
	{
		ALCint available = 0;
		alcGetIntegerv(device, ALC_CAPTURE_SAMPLES, 1, &available);
		int taken = std::min((int)available, count);
		if (taken <= 0)
			return 0;
		alcCaptureSamples(device, samples, taken);
		return taken;
	}

	void Stop() override { alcCaptureStop(device); }

private:
	ALCdevice* device;
};

std::unique_ptr<AudioCapture> AudioCapture::Open(int frequency, int samples)
{
	ALCdevice* device = alcCaptureOpenDevice(nullptr, frequency, AL_FORMAT_MONO16, samples);
	if (!device)
		return nullptr;
	alcCaptureStart(device);
	return std::make_unique<OpenALAudioCapture>(device);
}

// The device of a machine with no sound output: nothing is mixed or heard,
// and every voice is free, as no sound plays where the original's engine
// has dropped its audio subsystem (UEngine::InitAudio, Engine.dll
// 0x1037fe50). The subsystem's own work -- a sound's ID, which a script
// gets, the music's changes -- goes on as with a device.
class SilentAudioDevice : public AudioDevice
{
public:
	SilentAudioDevice(int numVoices) : numVoices(numVoices) {}

	void AddSound(USound* sound) override {}
	void RemoveSound(USound* sound) override {}
	bool HasSound(USound* sound) override { return true; }
	bool IsPlaying(int channel) override { return false; }
	int GetTotalChannels() override { return numVoices; }
	void PlayMusic(std::unique_ptr<AudioSource> source, float volume) override {}
	void PlayMixedSound(int channel, USound* sound, int volume, int panning, float pitch) override {}
	void UpdateMixedSound(int channel, int volume, int panning, float pitch) override {}
	void StopSound(int channel) override {}
	bool GetMixedSoundPosition(int channel, uint64_t& frame) override { return false; }
	bool ReadMixedSound(int channel, uint64_t frame, int16_t* frames, int count) override { return false; }
	void SetMusicVolume(float volume) override {}
	void SetSoundVolume(float volume) override {}
	void SetSpeechVolume(float volume) override {}
	int GetMusicOrder() override { return -1; }
	bool SetMusicOrder(int order, float volume, bool atOnce = false) override { return true; }
	void SetReverb(const ReverbSettings* settings) override {}
	void Update() override {}

private:
	int numVoices;
};

std::unique_ptr<AudioDevice> AudioDevice::CreateSilent(int numVoices)
{
	return std::make_unique<SilentAudioDevice>(numVoices);
}
