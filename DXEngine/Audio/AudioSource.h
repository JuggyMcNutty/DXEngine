
#pragma once

#include <memory>
#include <vector>

class AudioSource
{
public:
	static std::unique_ptr<AudioSource> CreateMp3(Array<uint8_t> filedata);
	static std::unique_ptr<AudioSource> CreateWav(Array<uint8_t> filedata);
	static std::unique_ptr<AudioSource> CreateOgg(Array<uint8_t> filedata, bool loop = false);
	static std::unique_ptr<AudioSource> CreateMod(Array<uint8_t> filedata, bool loop = true, int subsong = 0);

	AudioSource() = default;
	virtual ~AudioSource() = default;
	virtual int GetFrequency() = 0;
	virtual int GetChannels() = 0;
	virtual int GetSamples() = 0;
	virtual void SeekToSample(uint64_t position) = 0;
	virtual size_t ReadSamples(float* output, size_t samples) = 0;

	// Tracker music's position in its order list (UE1's SongSection); -1 and
	// a no-op for sources without orders.
	virtual int GetOrder() { return -1; }
	virtual void SetOrder(int order) {}

	// The sections a song Galaxy plays takes, its orders before the first
	// end mark (bugs.md AUD.17); -1 for a source that takes any order asked.
	virtual int GetSections() { return -1; }

	// Tracker music's tempo, 5/2 of its ticks a second; 0 for sources
	// without one.
	virtual double GetTempo() { return 0.0; }

	// The output mono, without UseStereo (bugs.md AUD.14): a source that
	// pans its own voices reads both channels as their mono mix, each voice
	// at the mean of its two sides as Galaxy's; a no-op for others, which
	// the device folds.
	virtual void SetMono(bool mono) {}

	// The rate the device mixes at, which a source that renders at any rate
	// takes as its own (GetFrequency); a no-op for others, which the device
	// reads between their frames.
	virtual void SetFrequency(int rate) {}

	bool bIsLooped = false;
	uint32_t loopStart = 0;
	uint32_t loopEnd = 0;
	bool eightBit = false;      // a WAV of 8-bit samples
};
