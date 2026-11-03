#pragma once

#include "UConEvent.h"

class UConEventAddSkillPoints : public UConEvent
{
public:
	using UConEvent::UConEvent;

	ScriptString& awardMessage() { return Value<ScriptString>(PropOffsets_ConEventAddSkillPoints.awardMessage); }
	int& pointsToAdd() { return Value<int>(PropOffsets_ConEventAddSkillPoints.pointsToAdd); }
};
