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

	// Galaxy pans a song's voices by the square roots of the pan, held to
	// 127 of 128 (0x106153d4, 0x10617770): OpenMPT's render.panning.galaxy,
	// DXEngine's own in Thirdparty/openmpt (bugs.md AUD.16). Its gain is the
	// Music slider's squared with the voice's own 100 (UDXEngineAudioDevice's
	// GalaxyMusicGain), the song's mix volume made 128.
	constexpr const char* PanningCtl = "render.panning.galaxy";

	// Galaxy's player gives the song its own speed, tempo and pans again as
	// it enters order 0 (0x10615710), pans by its 8xx, written as X with
	// Galaxy's parameter, a surround at a pan, a channel the header pans over
	// 64 a surround a quarter left (0x10616eb0, 0x10628990), and its section
	// change silences the voices and starts the section as the playing row
	// ends, the song not replayed to it, its order count the order before
	// until then (Update, 0x1060a822; 0x10616f29): OpenMPT's play.galaxy,
	// DXEngine's own in Thirdparty/openmpt (bugs.md AUD.16, AUD.17).
	constexpr const char* PlayerCtl = "play.galaxy";
}
