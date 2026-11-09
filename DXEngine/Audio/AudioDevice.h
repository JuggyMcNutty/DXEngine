#pragma once

#include "AudioSource.h"

#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include "Math/vec.h"

class AudioSource;
class USound;

// A zone's reverb, as Galaxy takes it (galaxy-dll.md, Reverb): the master
// gain, the high-frequency cutoff, and six echoes.
struct ReverbSettings
{
	float masterGain = 0.0f;        // 0-1
	float cutoffHz = 44100.0f;
	float delaySeconds[6] = {};     // each echo's delay, 0.001-0.34
	float gains[6] = {};            // each echo's gain, 0.001-0.999
};

class AudioDevice
{
public:
	// mixAheadMs: the output's mix-ahead (Galaxy's Latency), 10 ms buffers; stereo: else mono, as Galaxy's UseStereo off
	static std::unique_ptr<AudioDevice> Create(int frequency, int numVoices, int mixAheadMs, bool stereo);
	// A device that plays nothing, for a machine whose output will not start
	// (bugs.md AUD.11): its voices free, never playing.
	static std::unique_ptr<AudioDevice> CreateSilent(int numVoices);

	virtual ~AudioDevice() = default;
	virtual void AddSound(USound* sound) = 0;
	virtual void RemoveSound(USound* sound) = 0;
	virtual bool IsPlaying(int channel) = 0;
	virtual int GetTotalChannels() = 0;
	// A song in place of the one playing, or none, and the music's gain to
	// play it at: the gain reaches the output only once the old song's
	// sound is dropped, as Galaxy changes the song and sets its volume in one
	// locked Update (bugs.md AUD.12).
	virtual void PlayMusic(std::unique_ptr<AudioSource> source, float volume) = 0;
	// Deus Ex's sounds go through Galaxy's mixer instead (GalaxyMixer), each
	// with Galaxy's voice volume, 0-32767, and pan, 0-32767 from the left.
	virtual void PlayMixedSound(int channel, USound* sound, int volume, int panning, float pitch) = 0;
	virtual void UpdateMixedSound(int channel, int volume, int panning, float pitch) = 0;
	virtual void StopSound(int channel) = 0;
	// A mixed sound's voice as Galaxy's lip sync reads it (GalaxyMixer's
	// GetPosition and Read).
	virtual bool GetMixedSoundPosition(int channel, uint64_t& frame) = 0;
	virtual bool ReadMixedSound(int channel, uint64_t frame, int16_t* frames, int count) = 0;
	// The music's gain, linear, Galaxy's law the subsystem's
	// (UDXEngineAudioDevice::Update); it follows any change of song or order
	// handed over before it.
	virtual void SetMusicVolume(float volume) = 0;
	virtual void SetSoundVolume(float volume) = 0;
	virtual void SetSpeechVolume(float volume) = 0;
	// The playing music's order (UE1's SongSection), -1 while unknown, and a
	// jump to another order without reloading the song, at the gain given,
	// as PlayMusic takes one: for a song Galaxy plays, its order count the
	// order before until the section starts as the playing row ends, and
	// false for a section past the song's last order, which is refused
	// (bugs.md AUD.17).
	virtual int GetMusicOrder() = 0;
	virtual bool SetMusicOrder(int order, float volume) = 0;
	// The reverb every mixed sound plays through; nullptr turns it off.
	virtual void SetReverb(const ReverbSettings* settings) = 0;
	virtual void Update() = 0;
};
