#pragma once

#include <cstdint>

// An Impulse Tracker song as Galaxy.dll plays it (bugs.md AUD.10). Galaxy's
// IT loader (Galaxy.dll 0x10628990) converts a song into the commands of its
// own player (0x10616eb0) as it loads it, and plays only what it keeps; here
// the song is rewritten so, still an IT song, before OpenMPT reads it.
namespace GalaxyMusic
{
	// The song Galaxy's loader makes of an IT module; empty for data that is
	// none, which plays as it is.
	Array<uint8_t> ConvertIT(const Array<uint8_t>& song);

	// Galaxy pans a song's voices by the square roots of the pan
	// (0x106153d4), a centred channel at 0.707 a side, its gain the Music
	// slider's squared with the voice's own 100 (UDXEngineAudioDevice's
	// GalaxyMusicGain). OpenMPT, the mix volume made 128, pans linearly, a
	// centred channel at 0.5 a side: the song is played 3.01 dB up, so its
	// centred channels sound as Galaxy's. A channel panned hard to one side
	// then plays 3 dB over Galaxy's (bugs.md AUD.10).
	constexpr int GainMillibel = 301;
}
