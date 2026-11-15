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

	// None for a load with no travel info: the Entry level's
	void Init(const std::map<std::string, std::string>* travelInfo, const std::string& portal);
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

	// UHXGameEngine::Init's start (HX.dll 0x1003b100): the start URL the
	// command line's first word, with -hax0r or not; the map to fall back
	// to the ini's [URL] LocalMap, a dedicated server's [URL] Map; a
	// client's player named Player, or none, given a random <A-Y>_Denton,
	// written to the user ini; the save folder made; the game's packages'
	// GUIDs warned about when not the game's
	void Start(std::string& url, std::string& fallbackMap, bool dedicated);

	// Every actor of the level whose class has a bool bSpawning gets it
	// True, as HX's LoadMap marks them before the level begins
	void SetSpawning(ULevel* level);

	// HXSpawnCoopStarts (HX.dll 0x10025e20), as HX's LoadMap calls it right
	// after the map's load for a level with no game: the starts HX.dll holds
	// for the map, read from the player's own HX.dll (HXCoopStarts.h), each
	// spawned with its tag, its settings and its collision
	void SpawnCoopStarts(ULevelInfo* levelInfo, const std::string& map);

	// CoopSetupActors (HX.dll 0x10043f80): the map's actors made fit for
	// HX's game -- decorations, effects, projectiles and triggers no longer
	// static, the level info and the camera and interpolation points kept
	// on each side, the movers made HX's own
	void SetupActors(ULevel* level);

	// The Steve spawned beside a new game (HX's LoadMap), its flag base a
	// new HXFlagBase, read from the level's travel info (none for the Entry
	// level's load, as the original's Init loads it with none)
	UHXSteve* SpawnSteve(ULevel* level, ULevelInfo* levelInfo, const std::map<std::string, std::string>* travelInfo, const std::string& portal);

	// Browse's "Adding Steve to TravelInfo.": every Steve of the level
	// written into the travel info
	void ExportSteves(ULevel* level, std::map<std::string, std::string>& travelInfo);

	// UHXLevel::Tick's distances, before the actors tick (HX.dll
	// 0x10049620): on a client each dynamic actor's from the viewport's
	// player; alone or on a server the nearest HX player's, an HX NPC's
	// PlayerDistances filled with every player's, nearest first
	void SetDistances(ULevel* level);

	// UHXModel::FastRepLineCheck (HX.dll 0x100524b0) down a level's BSP,
	// the movers filed in: whether the line is clear, seeing through
	// invisible, masked, translucent and modulated surfaces
	bool FastRepLineCheck(ULevel* level, const vec3& end, const vec3& start);

	// ExitLevelCleanup (HX.dll 0x10044630), as a map's load leaves the old
	// level: the conversation cameras' and events' actors, the flags and
	// flag bases, the notes, goals and logs let go, those flags, bases,
	// notes, goals and logs and the cameras eliminated
	void ExitLevelCleanup();
}
