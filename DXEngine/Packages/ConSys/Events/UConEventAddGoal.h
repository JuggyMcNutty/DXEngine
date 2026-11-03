#pragma once

#include "UConEvent.h"

class UConEventAddGoal : public UConEvent
{
public:
	using UConEvent::UConEvent;

	BitfieldBool bGoalCompleted() { return BoolValue(PropOffsets_ConEventAddGoal.bGoalCompleted); }
	BitfieldBool bPrimaryGoal() { return BoolValue(PropOffsets_ConEventAddGoal.bPrimaryGoal); }
	NameString& goalName() { return Value<NameString>(PropOffsets_ConEventAddGoal.goalName); }
	ScriptString& goalText() { return Value<ScriptString>(PropOffsets_ConEventAddGoal.goalText); }
};
