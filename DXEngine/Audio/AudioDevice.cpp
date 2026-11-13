
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

		// The device closed again before a failure is thrown: the engine
		// plays on without it (UDXEngineAudioDevice::InitDevice)
		alContext = alcCreateContext(alDevice, ctxAttribs);
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
		mixedSounds[sound] = std::move(sample);
	}

	void RemoveSound(USound* sound) override
	{
		mixedSounds.erase(sound);
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
		mixer->Play(channel, it != mixedSounds.end() ? it->second : nullptr, volume, panning, pitch);
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
		std::string failure;
		{
			std::unique_lock lock(musicThreadData.mutex);
			failure.swap(musicThreadData.failure);
		}
		if (!failure.empty())
			LogMessage("Music: the song stopped: " + failure);
	}

	// The song's next frames at the mix rate, stereo: a source at another
	// rate read between its frames linearly, from its first (a place of 2
	// reads it and the next), a mono one in both channels. What a source
	// that ends or reads short leaves is silence.
	void ReadMusic(AudioSource* music, float* out, int frames)
	{
		int channels = std::clamp(music->GetChannels(), 1, 2);
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
				float frame[2] = {};
				if (music->ReadSamples(frame, channels) < (size_t)channels)
					frame[0] = frame[1] = 0.0f;
				musicNext[0] = frame[0];
				musicNext[1] = channels == 2 ? frame[1] : frame[0];
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
				if (!stereo && currentMusic->GetChannels() == 2)
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
		mixThread = std::thread([this]() { MixThreadMain(); });
	}

	void StopMixStream()
	{
		mixExit = true;
		mixThread.join();
		alSourceStop(alMixSource);
		alSourcei(alMixSource, AL_BUFFER, 0);
		alDeleteSources(1, &alMixSource);
		alDeleteBuffers((ALsizei)alMixBuffers.size(), &alMixBuffers[0]);
	}

	void MixThreadMain()
	{
		std::vector<float> block(mixBufferFrames * 2);
		auto queue = [&](ALuint buffer) {
			mixer->Mix(block.data(), mixBufferFrames);
			MixMusic(block.data(), mixBufferFrames);
			alBufferData(buffer, AL_FORMAT_STEREO_FLOAT32, block.data(), (ALsizei)(block.size() * sizeof(float)), frequency);
			alSourceQueueBuffers(alMixSource, 1, &buffer);
		};

		for (ALuint buffer : alMixBuffers)
			queue(buffer);
		alSourcePlay(alMixSource);

		while (!mixExit)
		{
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

			using namespace std::chrono_literals;
			std::this_thread::sleep_for(2ms);
		}
	}

	ALCdevice* alDevice = nullptr;
	ALCcontext* alContext = nullptr;

	// Deus Ex's mixer, the sounds it holds, and its output
	std::unique_ptr<GalaxyMixer> mixer;
	std::unordered_map<USound*, std::shared_ptr<const GalaxySample>> mixedSounds;
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
	} musicThreadData;

	// The mix thread's song, and its place between a source's frames
	std::unique_ptr<AudioSource> currentMusic;
	std::vector<float> musicBlock;
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
