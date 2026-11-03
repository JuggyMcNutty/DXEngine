#pragma once

#include "Packages/Core/UObject.h"

class ULevelSummary : public UObject
{
public:
	using UObject::UObject;

	ScriptString& Author() { return Value<ScriptString>(PropOffsets_LevelSummary.Author); }
	ScriptString& IdealPlayerCount() { return Value<ScriptString>(PropOffsets_LevelSummary.IdealPlayerCount); }
	ScriptString& LevelEnterText() { return Value<ScriptString>(PropOffsets_LevelSummary.LevelEnterText); }
	int& RecommendedEnemies() { return Value<int>(PropOffsets_LevelSummary.RecommendedEnemies); }
	int& RecommendedTeammates() { return Value<int>(PropOffsets_LevelSummary.RecommendedTeammates); }
	ScriptString& Title() { return Value<ScriptString>(PropOffsets_LevelSummary.Title); }
};
