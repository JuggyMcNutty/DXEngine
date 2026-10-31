
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
	OpenALAudioDevice(int inFrequency, int numVoices, int inMusicBufferCount, int inMusicBufferSize, int mixAheadMs, bool inStereo)
	{
		frequency = inFrequency;
		musicBufferCount = inMusicBufferCount;
		musicBufferSize = inMusicBufferSize;
		mixBufferCount = std::clamp(mixAheadMs / 10, 2, 25);
		stereo = inStereo;

		musicBuffer.resize(musicBufferSize * musicBufferCount);
		musicQueue.Resize(musicBufferCount);

		for (int i = 0; i < musicBufferCount; i++)
		{
			musicQueue.Push(&musicBuffer[musicBufferSize * i]);
			musicQueue.Pop();
		}

		// Init OpenAL, on the system's default device
		alDevice = alcOpenDevice(NULL);
		if (alDevice == nullptr)
			Exception::Throw("Failed to initialize OpenAL device");

		// The context at the mixing rate chosen (the launcher's), so OpenAL
		// mixes the sources at it rather than resampling a mix made at it
		const ALCint ctxAttribs[] =
		{
			ALC_FREQUENCY, inFrequency,
			ALC_REFRESH, 60,
			ALC_SYNC, ALC_FALSE,
			0
		};

		alContext = alcCreateContext(alDevice, ctxAttribs);
		if (alContext == nullptr)
			Exception::Throw("Failed to initialize OpenAL context");

		if (alcMakeContextCurrent(alContext) == ALC_FALSE)
			Exception::Throw("Failed to make OpenAL context current");

		// Deus Ex's sounds are mixed as Galaxy mixes them, in Galaxy's 32
		// channel records, the mix streamed through one source at OutputRate,
		// as Galaxy's goes out, so its reverb's delays are the original's
		// samples; nothing is spatialized by OpenAL.
		mixer = std::make_unique<GalaxyMixer>(frequency, 32);
		StartMixStream();

		// init music source/buffer
		alGenSources(1, &alMusicSource);
		alSourcei(alMusicSource, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);

		alMusicBuffers.resize(musicBufferCount);
		alGenBuffers(musicBufferCount, &alMusicBuffers[0]);

		// init playback thread
		musicThreadData.thread = std::thread([this]() { MusicThreadMain(); });
	}

	~OpenALAudioDevice()
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.exitFlag = true;
		lock.unlock();
		musicThreadData.thread.join();

		StopMixStream();

		alSourceStop(alMusicSource);
		alDeleteSources(1, &alMusicSource);

		alDeleteBuffers((ALsizei)alMusicBuffers.size(), &alMusicBuffers[0]);

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
		// its start here (bugs.md D.32).
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

	void PlayMusic(std::unique_ptr<AudioSource> source) override
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.music = std::move(source);
		musicThreadData.musicUpdate = true;
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

	// A jump is the order at once, as Galaxy's Update jumps and writes the
	// order back in one step: the main thread reads it that frame, not the
	// pre-jump order until the music thread's next pass
	void SetMusicOrder(int order) override
	{
		std::unique_lock lock(musicThreadData.mutex);
		musicThreadData.jumpOrder = order;
		if (order >= 0)
			musicThreadData.currentOrder.store(order);
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

	static std::string getALErrorString(ALenum error)
	{
		switch (error)
		{
		case AL_NO_ERROR:
			return "AL_NO_ERROR";
		case AL_INVALID_NAME:
			return "AL_INVALID_NAME";
		case AL_INVALID_ENUM:
			return "AL_INVALID_ENUM";
		case AL_INVALID_VALUE:
			return "AL_INVALID_VALUE";
		case AL_INVALID_OPERATION:
			return "AL_INVALID_OPERATION";
		case AL_OUT_OF_MEMORY:
			return "AL_OUT_OF_MEMORY";
		default:
			return "AL_UNKNOWN_ERROR";
		}
	}

	// The source stopped and every chunk rendered ahead dropped, so a new
	// song or a jump starts with its own sound, not the old one's
	void StopMusicBuffers(bool& musicPlaying)
	{
		if (musicPlaying)
		{
			alSourceStop(alMusicSource);
			// What is queued, all processed once stopped: an unqueue of all 16
			// with fewer queued (a throw mid-way) was refused whole, the
			// buffers staying queued and the next song's first upload failing
			// for the rest of the session
			ALint queued = 0;
			alGetSourcei(alMusicSource, AL_BUFFERS_QUEUED, &queued);
			if (queued > 0)
			{
				std::vector<ALuint> unqueued((size_t)queued);
				alSourceUnqueueBuffers(alMusicSource, queued, unqueued.data());
			}
			alGetError();
			musicPlaying = false;
		}
		while (!musicQueue.Empty())
			musicQueue.Pop();
	}

	// On the music thread, which must not touch the script's call stack as
	// Exception::Throw does: a plain error, which MusicThreadMain catches.
	static void CheckMusicALError(const char* call)
	{
		ALenum error = alGetError();
		if (error != AL_NO_ERROR)
			throw std::runtime_error(std::string(call) + " failed: " + getALErrorString(error));
	}

	void PlayMusicBuffer(AudioSource* music)
	{
		int format = (music->GetChannels() == 1) ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_STEREO_FLOAT32;
		int freq = music->GetFrequency();

		// The context's error state is one for every thread and keeps its
		// first error until read: another thread's (the listener's calls, a
		// mix upload's) is dropped here, not blamed on the song
		alGetError();

		for (int i = 0; i < musicBufferCount; i++)
		{
			alBufferData(alMusicBuffers[i], format, musicQueue.Pop(), musicBufferSize*4, freq);
			CheckMusicALError("alBufferData");

			alSourceQueueBuffers(alMusicSource, 1, &alMusicBuffers[i]);
			CheckMusicALError("alSourceQueueBuffers");
		}

		alSourcePlay(alMusicSource);
		CheckMusicALError("alSourcePlay");
	}

	void UpdateMusicBuffer(AudioSource* music)
	{
		alGetError(); // another thread's error dropped, as PlayMusicBuffer drops it
		ALint status;
		alGetSourcei(alMusicSource, AL_BUFFERS_PROCESSED, &status);

		while (status)
		{
			ALuint buffer;
			alSourceUnqueueBuffers(alMusicSource, 1, &buffer);

			int format = (music->GetChannels() == 1) ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_STEREO_FLOAT32;
			int freq = music->GetFrequency();

			alBufferData(buffer, format, musicQueue.Pop(), musicBufferSize*4, freq);
			CheckMusicALError("alBufferData");

			alSourceQueueBuffers(alMusicSource, 1, &buffer);
			CheckMusicALError("alSourceQueueBuffers");

			status--;
		}

		alGetSourcei(alMusicSource, AL_SOURCE_STATE, &status);
		if (status == AL_STOPPED)
		{
			alSourcePlay(alMusicSource);
			CheckMusicALError("alSourcePlay");
		}
	}

	void SetMusicVolume(float volume) override
	{
		alSourcef(alMusicSource, AL_GAIN, volume);
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

	void MusicThreadMain()
	{
		std::unique_ptr<AudioSource> currentMusic;
		bool musicPlaying = false;

		while (true)
		{
			// Lock the mutex. Grab the data we need from the main thread. Then unlock.
			std::unique_lock lock(musicThreadData.mutex);
			if (musicThreadData.exitFlag)
				break;
			if (musicThreadData.musicUpdate)
			{
				StopMusicBuffers(musicPlaying);
				currentMusic = std::move(musicThreadData.music);
				musicThreadData.musicUpdate = false;
				mixer->SetMusicTempo(0.0);
			}
			int jumpOrder = musicThreadData.jumpOrder;
			int generation = musicThreadData.generation;
			musicThreadData.jumpOrder = -1;
			lock.unlock();
			// Nothing of musicThreadData is touched past this point but under
			// the lock again (the order's store, which is this song's alone:
			// a new song set meanwhile keeps PlayMusic's -1).

			if (jumpOrder >= 0 && currentMusic)
			{
				// What was rendered from the old position is not played
				StopMusicBuffers(musicPlaying);
				currentMusic->SetOrder(jumpOrder);
			}

			if (currentMusic)
			{
				// A song that fails stops, and the game goes on without it: the
				// failure is left for the main thread to log
				try
				{
					{
						std::unique_lock orderLock(musicThreadData.mutex);
						if (musicThreadData.generation == generation)
							musicThreadData.currentOrder.store(currentMusic->GetOrder());
					}

					while (musicQueue.Size() < musicBufferCount)
					{
						// render a chunk of music; what a source that ends or reads
						// short leaves is silence, not what the chunk held 16
						// chunks before
						float* chunk = musicQueue.GetNextFree();
						size_t got = currentMusic->ReadSamples(chunk, musicBufferSize);
						if (got < (size_t)musicBufferSize)
							std::fill(chunk + got, chunk + musicBufferSize, 0.0f);
						musicQueue.Push(musicQueue.GetNextFree());
					}
					mixer->SetMusicTempo(currentMusic->GetTempo());

					if (!musicPlaying)
					{
						PlayMusicBuffer(currentMusic.get());
						musicPlaying = true;
					}
					else if (musicQueue.Size() > 0)
					{
						UpdateMusicBuffer(currentMusic.get());
					}
				}
				catch (const std::exception& e)
				{
					musicPlaying = true;
					StopMusicBuffers(musicPlaying);
					alGetError();
					currentMusic.reset();
					mixer->SetMusicTempo(0.0);
					musicThreadData.currentOrder.store(-1);
					std::unique_lock failedLock(musicThreadData.mutex);
					musicThreadData.failure = e.what();
				}
			}
			else
			{
				musicPlaying = false;
			}
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(5ms);
		}
	}

	// The mixer's output: one stereo source, straight to the speakers, its
	// queue 10 ms buffers deep to Latency, Galaxy's mix-ahead (40 ms by
	// default); mono folded from both channels without UseStereo, as Galaxy
	// mixes it.
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
			if (!stereo)
			{
				for (int i = 0; i < mixBufferFrames; i++)
				{
					float mono = 0.5f * (block[2 * i] + block[2 * i + 1]);
					block[2 * i] = block[2 * i + 1] = mono;
				}
			}
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
	ALuint alMusicSource = 0;
	Array<ALuint> alMusicBuffers;

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

	template<class T> class RingQueue
	{
	public:
		RingQueue()
		{
			Data = nullptr;
			Len = 0;
			Num = 0;
			Current = 0;
		}

		RingQueue(size_t n)
		{
			Data = static_cast<T*>(malloc(sizeof(T) * n));
			Len = n;
			Num = 0;
			Current = 0;
		}

		RingQueue(size_t n, const T& Value)
		{
			Data = static_cast<T*>(malloc(sizeof(T) * n));
			for (int i = 0; i < n; i++)
				Data[i] = Value;

			Len = n;
			Num = 0;
			Current = 0;
		}

		~RingQueue()
		{
			free(Data);
		}

		bool Empty()
		{
			return (Num == 0);
		}

		size_t Size()
		{
			return Num;
		}

		T& Front()
		{
			return Data[Current];
		}

		T& GetNextFree()
		{
			size_t Index = Current + Num;
			if (Index >= Len)
				Index -= Len;

			return Data[Index];
		}

		bool Push(const T& Val)
		{
			if (Num == Len)
				return false;

			GetNextFree() = Val;
			Num++;

			return true;
		}

		bool Push(T& Val)
		{
			if (Num == Len)
				return false;

			GetNextFree() = Val;
			Num++;

			return true;
		}

		T& Pop()
		{
			T& Out = Data[Current];
			if (Num > 0)
			{
				Current++;
				if (Current >= Len)
					Current = 0;

				Num--;
			}
			return Out;
		}

		bool Resize(size_t NewSize)
		{
			T* NewData = static_cast<T*>(realloc(Data, sizeof(T) * NewSize));
			if (NewData == NULL)
				return false;

			Data = NewData;
			Len = NewSize;
			return true;
		}

		void Clear()
		{
			Current = 0;
			Num = 0;
		}

	private:
		T* Data = nullptr;
		size_t Len = 0;
		size_t Current = 0;
		size_t Num = 0;
	};

	int frequency = 48000;

	// Note: variables changed in musicThreadData *must* be done within a mutex lock to be thread safe
	struct
	{
		std::mutex mutex;
		std::thread thread;
		bool exitFlag = false;
		std::unique_ptr<AudioSource> music;
		bool musicUpdate = false;
		int jumpOrder = -1;                     // an order to jump the playing song to
		std::atomic<int> currentOrder{ -1 };    // the order playing: a jump's at once, else the music thread's
		int generation = 0;                     // counts the songs set, so the thread's order is its own song's
		std::string failure;                    // why the song stopped, for the main thread to log
	} musicThreadData;

	RingQueue<float*> musicQueue;
	int musicBufferCount = 0;
	int musicBufferSize = 0;
	Array<float> musicBuffer;
	float currentMusicVolume = 0.0f;
	float targetMusicVolume = 0.0f;
	float fadeRate = 0.0f;
	float globalSoundVolume = 1.0f;
	float globalSpeechVolume = 1.0f;
};

std::unique_ptr<AudioDevice> AudioDevice::Create(int frequency, int numVoices, int musicBufferCount, int musicBufferSize, int mixAheadMs, bool stereo)
{
	return std::make_unique<OpenALAudioDevice>(frequency, numVoices, musicBufferCount, musicBufferSize, mixAheadMs, stereo);
}
