#pragma once

#include "Packages/Engine/Actors/Info/UInfo.h"

class UDeusExLevelInfo : public UInfo
{
public:
	using UInfo::UInfo;

	ScriptString& MapName() { return Value<ScriptString>(PropOffsets_DeusExLevelInfo.MapName); }
	ScriptString& MapAuthor() { return Value<ScriptString>(PropOffsets_DeusExLevelInfo.MapAuthor); }
	ScriptString& MissionLocation() { return Value<ScriptString>(PropOffsets_DeusExLevelInfo.MissionLocation); }
	int& MissionNumber() { return Value<int>(PropOffsets_DeusExLevelInfo.missionNumber); }
	BitfieldBool bMultiPlayerMap() { return BoolValue(PropOffsets_DeusExLevelInfo.bMultiPlayerMap); }
	// MissionScript* Script() { return Value<MissionScript>(PropOffsets_DeusExLevelInfo.Script); } // MissionScript isn't a native class
	int& TrueNorth() { return Value<int>(PropOffsets_DeusExLevelInfo.TrueNorth); }
	FixedArrayView<std::string, 4> StartupMessage() { return FixedArray<std::string, 4>(PropOffsets_DeusExLevelInfo.startupMessage); }
	ScriptString& ConversationPackage() { return Value<ScriptString>(PropOffsets_DeusExLevelInfo.ConversationPackage); }
};
