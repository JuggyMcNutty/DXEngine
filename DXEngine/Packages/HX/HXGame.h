#pragma once

#include "Packages/Engine/Actors/UActor.h"
#include <map>

class ULevel;
class ULevelInfo;

// HX's Steve (AHXSteve): the campaign's state a level hands the next -- the
// flags, the skill points, the goals, the nano keys and the images --
// written into the level's travel info and read back from it, in HX's own
// lines (HX.dll 0x10022c00, 0x10023cb0).
class UHXSteve : public UActor
{
public:
	using UActor::UActor;

	void Init(const std::map<std::string, std::string>& travelInfo, const std::string& portal);
	void ExportTravel(std::map<std::string, std::string>& travelInfo);

private:
	void InitFlagBase(const std::string& text);
	void InitGoals(const std::string& text);
	void InitNanoKeys(const std::string& text);
	void InitImages(const std::string& text);
};

// HX's game engine (UHXGameEngine), whose part DXEngine plays when the ini's
// GameEngine= names it: the steps of its map load and travel DXEngine's own
// take in its place (dx-reverse-info hx-dll.md).
namespace HXGameEngine
{
	// Whether the ini's GameEngine= is HX.HXGameEngine, read once
	bool Active();

	// Every actor of the level whose class has a bool bSpawning gets it
	// True, as HX's LoadMap marks them before the level begins
	void SetSpawning(ULevel* level);

	// CoopSetupActors (HX.dll 0x10043f80): the map's actors made fit for
	// HX's game -- decorations, effects, projectiles and triggers no longer
	// static, the level info and the camera and interpolation points kept
	// on each side, the movers made HX's own
	void SetupActors(ULevel* level);

	// The Steve spawned beside a new game (HX's LoadMap), its flag base a
	// new HXFlagBase, read from the level's travel info
	UHXSteve* SpawnSteve(ULevel* level, ULevelInfo* levelInfo, const std::map<std::string, std::string>& travelInfo, const std::string& portal);

	// Browse's "Adding Steve to TravelInfo.": every Steve of the level
	// written into the travel info
	void ExportSteves(ULevel* level, std::map<std::string, std::string>& travelInfo);
}
