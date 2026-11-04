#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

struct ReverbSettings;

// A sound as Galaxy's mixer holds it: 16-bit frames, zeros past the end.
struct GalaxySample
{
	std::vector<int16_t> data;      // interleaved when stereo
	int channels = 1;
	uint64_t frames = 0;
	int rate = 0;
	bool looped = false;
	uint64_t loopStart = 0;
	uint64_t loopEnd = 0;           // the first frame not in the loop
	bool ringed = false;            // played from a ring its decoder fills: an MP3, a 16-bit stereo sound
};

// Galaxy.dll's sample mixer and zone reverb, as Deus Ex's sounds play
// (galaxy-dll.md, Each frame and Reverb): a channel's volume and pan come
// worked out by the subsystem, and the mixer gains the two sides by the
// square roots of the pan, resamples linearly and sends everything to the
// reverb, whose output joins the mix at the zone's master gain.
class GalaxyMixer
{
public:
	GalaxyMixer(int rate, int channels);

	int GetChannels() const { return (int)voices.size(); }

	// volume is Galaxy's voice volume, 0-32767; panning 0-32767 from left to
	// right, 49152 a sound behind with UseSurround.
	void Play(int channel, std::shared_ptr<const GalaxySample> sample, int volume, int panning, float pitch);
	void Update(int channel, int volume, int panning, float pitch);
	void Stop(int channel);
	bool IsPlaying(int channel);

	// A playing voice as Galaxy's lip sync reads it (galaxy-dll.md, Lip
	// sync): the frame of its sample its mix has reached, and count frames of
	// the sample's first channel from frame on, 0 past its end; false when
	// the channel plays nothing.
	bool GetPosition(int channel, uint64_t& frame);
	bool Read(int channel, uint64_t frame, int16_t* frames, int count);

	// Galaxy's sample volume: the louder of the Sound and Speech sliders,
	// 0 to 1.
	void SetMasterVolume(float louderSlider);

	// Galaxy's gain, 0-32767, for a volume of 0-127 it is handed -- the
	// samples' (glxSetSampleVolume) or the music's (glxSetMusicVolume) --,
	// as its mixer squares it on every voice (Galaxy.dll 0x106152df): twice
	// it squared, times the voice's own volume, twice its own byte of 0-127
	// squared (0x10617f50), then through four more full-scale factors of
	// 32767. A sound's voice is at 127, 32,258, as glxInit sets every voice;
	// a song's at 100, 20,000, as its load leaves every music voice
	// (0x1060e850): 31,751 and 19,684 at 127.
	static int SquaredGain(int volume, int voiceVolume);

	// The tempo of the module the music plays, which times Galaxy's tick;
	// 0 with none.
	void SetMusicTempo(double tempo);

	// nullptr, or a master gain of 0, turns the reverb off.
	void SetReverb(const ReverbSettings* settings);

	// Interleaved stereo, overwritten.
	void Mix(float* output, int frames);

private:
	struct Voice
	{
		std::shared_ptr<const GalaxySample> sample;
		bool playing = false;
		uint64_t position = 0;      // 16.16 frames
		uint32_t step = 0;          // 16.16 frames per output frame
		int volume = 0;
		int panning = 16383;
		uint64_t end = 0;           // the frame it stops at
		uint64_t fill = 0;          // a ringed voice: the frame its ring is filled to, 0 before the first fill
		int64_t tick = 0;           // a ringed voice: 16.16 output frames to its decoder's next call
		uint64_t turn = 0;          // a ringed voice: the ring's last frame in this turn
	};

	struct Reverb
	{
		static constexpr int Length = 16384;
		int position = 0;
		int delay[6] = {};
		float allpass[6] = {};      // g, -g and 1 - g^2 of each echo's filter
		float negAllpass[6] = {};
		float pass[6] = {};
		float lowpass[6] = {};      // a and 1 - a of its lowpass
		float keep[6] = {};
		float lowpassState[6] = {};
		float last[2] = {};
		float volume = 0.0f;
		std::vector<float> lines;   // three stages of Length stereo frames
	};

	void SetPitch(Voice& voice, float pitch);
	void FillRing(Voice& voice);
	void MixVoice(Voice& voice, float* dry, float* send, int frames);
	void RunReverb(float* dry, const float* send, int frames);

	std::mutex mutex;
	int rate = 44100;
	std::vector<Voice> voices;
	int master = 0;                 // the voices' common gain, 0-32767
	std::atomic<int64_t> tickPeriod{ 0 };   // 16.16 output frames between Galaxy's ticks
	bool reverbOn = false;
	float reverbKey[15] = {};
	Reverb reverb;
	std::vector<float> sendBuffer;
};
