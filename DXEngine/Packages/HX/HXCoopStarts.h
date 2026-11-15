#pragma once

#include "Math/vec.h"
#include "Math/rotator.h"

// HX's co-op starts as the player's own HX.dll holds them (bugs.md HX.6,
// dx-reverse-info hx-dll.md's co-op starts). They are code there, not data:
// HXSpawnCoopStarts (HX.dll 0x10025e20) tests the map's name against each of
// 80 maps' in turn, and for each start of the map that matches calls
// HXSpawnSingleStart (0x10025cb0) with sixteen arguments, every one a
// constant pushed before the call. So DXEngine runs that function for the
// map, as far as its instructions go -- the pushes, the name tests and their
// jumps, the calls it knows -- and takes each call's arguments as a start.
// DXEngine holds none of them: they are HX's.
namespace HXCoopStarts
{
	struct Start
	{
		std::string tag;
		vec3 location = vec3(0.0f);
		Rotator rotation = Rotator(0, 0, 0);
		int priority = 0;
		bool defaultStart = false;
		uint8_t teamNumber = 0;
		bool initiallyEnabled = false;
		std::string enableTag;
		std::string disableTag;
		std::string enabledMessage;
		bool collideActors = false;
	};

	// HX 0.9.89.4's HX.dll, the one build whose code Read follows, by its SHA1
	extern const char* const DllSha1;

	// The starts HX.dll's HXSpawnCoopStarts gives the map, in its order;
	// none for a map it does not name. False, with why, for bytes that are
	// not a 32-bit PE image holding the function, or for code that goes
	// where the reader does not follow (another build's).
	bool Read(const Array<uint8_t>& dll, const std::string& map, Array<Start>& starts, std::string& error);
}
