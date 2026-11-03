#pragma once

#include "UZoneInfo.h"
#include "UnrealURL.h"

class UGameInfo;
class UNavigationPoint;
class UPawn;
class UMusic;
class USpawnNotify;
class ULevelSummary;

enum ELevelAction
{
	LEVACT_None,
	LEVACT_Loading,
	LEVACT_Saving,
	LEVACT_Connecting,
	LEVACT_Precaching
};

class ULevelInfo : public UZoneInfo
{
public:
	using UZoneInfo::UZoneInfo;

	void InitActorZone() override;
	void UpdateActorZone() override;

	// A level's LevelInfo is in its own zone, in no leaf, zone 0 -- where
	// it is aside -- as the original's SetActorZone sets it (Engine.dll
	// 0x1039b940)
	PointRegion OwnRegion() { return { this, -1, 0 }; }

	PointRegion GetLocZone(const vec3& pos);

	UnrealURL URL;

	FixedArrayView<int, 8> AIProfile() { return FixedArray<int, 8>(PropOffsets_LevelInfo.AIProfile); }
	ScriptString& Author() { return Value<ScriptString>(PropOffsets_LevelInfo.Author); }
	float& AvgAITime() { return Value<float>(PropOffsets_LevelInfo.AvgAITime); }
	float& Brightness() { return Value<float>(PropOffsets_LevelInfo.Brightness); }
	uint8_t& CdTrack() { return Value<uint8_t>(PropOffsets_LevelInfo.CdTrack); }
	ScriptString& ComputerName() { return Value<ScriptString>(PropOffsets_LevelInfo.ComputerName); }
	int& Day() { return Value<int>(PropOffsets_LevelInfo.Day); }
	int& DayOfWeek() { return Value<int>(PropOffsets_LevelInfo.DayOfWeek); }
	UClass*& DefaultGameType() { return Value<UClass*>(PropOffsets_LevelInfo.DefaultGameType); }
	UTexture*& DefaultTexture() { return Value<UTexture*>(PropOffsets_LevelInfo.DefaultTexture); }
	ScriptString& EngineVersion() { return Value<ScriptString>(PropOffsets_LevelInfo.EngineVersion); }
	UGameInfo*& Game() { return Value<UGameInfo*>(PropOffsets_LevelInfo.Game); }
	int& Hour() { return Value<int>(PropOffsets_LevelInfo.Hour); }
	int& HubStackLevel() { return Value<int>(PropOffsets_LevelInfo.HubStackLevel); }
	ScriptString& IdealPlayerCount() { return Value<ScriptString>(PropOffsets_LevelInfo.IdealPlayerCount); }
	uint8_t& LevelAction() { return Value<uint8_t>(PropOffsets_LevelInfo.LevelAction); }
	ScriptString& LevelEnterText() { return Value<ScriptString>(PropOffsets_LevelInfo.LevelEnterText); }
	ScriptString& LocalizedPkg() { return Value<ScriptString>(PropOffsets_LevelInfo.LocalizedPkg); }
	int& Millisecond() { return Value<int>(PropOffsets_LevelInfo.Millisecond); }
	ScriptString& MinNetVersion() { return Value<ScriptString>(PropOffsets_LevelInfo.MinNetVersion); }
	int& Minute() { return Value<int>(PropOffsets_LevelInfo.Minute); }
	int& Month() { return Value<int>(PropOffsets_LevelInfo.Month); }
	UNavigationPoint*& NavigationPointList() { return Value<UNavigationPoint*>(PropOffsets_LevelInfo.NavigationPointList); }
	uint8_t& NetMode() { return Value<uint8_t>(PropOffsets_LevelInfo.NetMode); }
	float& NextSwitchCountdown() { return Value<float>(PropOffsets_LevelInfo.NextSwitchCountdown); }
	ScriptString& NextURL() { return Value<ScriptString>(PropOffsets_LevelInfo.NextURL); }
	ScriptString& Pauser() { return Value<ScriptString>(PropOffsets_LevelInfo.Pauser); }
	UPawn*& PawnList() { return Value<UPawn*>(PropOffsets_LevelInfo.PawnList); }
	float& PlayerDoppler() { return Value<float>(PropOffsets_LevelInfo.PlayerDoppler); }
	int& RecommendedEnemies() { return Value<int>(PropOffsets_LevelInfo.RecommendedEnemies); }
	int& RecommendedTeammates() { return Value<int>(PropOffsets_LevelInfo.RecommendedTeammates); }
	UTexture*& Screenshot() { return Value<UTexture*>(PropOffsets_LevelInfo.Screenshot); }
	int& Second() { return Value<int>(PropOffsets_LevelInfo.Second); }
	UMusic*& Song() { return Value<UMusic*>(PropOffsets_LevelInfo.Song); }
	uint8_t& SongSection() { return Value<uint8_t>(PropOffsets_LevelInfo.SongSection); }
	USpawnNotify*& SpawnNotify() { return Value<USpawnNotify*>(PropOffsets_LevelInfo.SpawnNotify); }
	ULevelSummary*& Summary() { return Value<ULevelSummary*>(PropOffsets_LevelInfo.Summary); }
	float& TimeDilation() { return Value<float>(PropOffsets_LevelInfo.TimeDilation); }
	float& TimeSeconds() { return Value<float>(PropOffsets_LevelInfo.TimeSeconds); }
	ScriptString& Title() { return Value<ScriptString>(PropOffsets_LevelInfo.Title); }
	ScriptString& VisibleGroups() { return Value<ScriptString>(PropOffsets_LevelInfo.VisibleGroups); }
	int& Year() { return Value<int>(PropOffsets_LevelInfo.Year); }
	BitfieldBool bAggressiveLOD() { return BoolValue(PropOffsets_LevelInfo.bAggressiveLOD); }
	BitfieldBool bAllowFOV() { return BoolValue(PropOffsets_LevelInfo.bAllowFOV); }
	BitfieldBool bBegunPlay() { return BoolValue(PropOffsets_LevelInfo.bBegunPlay); }
	BitfieldBool bCheckWalkSurfaces() { return BoolValue(PropOffsets_LevelInfo.bCheckWalkSurfaces); }
	BitfieldBool bDropDetail() { return BoolValue(PropOffsets_LevelInfo.bDropDetail); }
	BitfieldBool bHighDetailMode() { return BoolValue(PropOffsets_LevelInfo.bHighDetailMode); }
	BitfieldBool bHumansOnly() { return BoolValue(PropOffsets_LevelInfo.bHumansOnly); }
	BitfieldBool bLonePlayer() { return BoolValue(PropOffsets_LevelInfo.bLonePlayer); }
	BitfieldBool bLowRes() { return BoolValue(PropOffsets_LevelInfo.bLowRes); }
	BitfieldBool bNeverPrecache() { return BoolValue(PropOffsets_LevelInfo.bNeverPrecache); }
	BitfieldBool bNextItems() { return BoolValue(PropOffsets_LevelInfo.bNextItems); }
	BitfieldBool bNoCheating() { return BoolValue(PropOffsets_LevelInfo.bNoCheating); }
	BitfieldBool bPlayersOnly() { return BoolValue(PropOffsets_LevelInfo.bPlayersOnly); }
	BitfieldBool bStartup() { return BoolValue(PropOffsets_LevelInfo.bStartup); }
};
