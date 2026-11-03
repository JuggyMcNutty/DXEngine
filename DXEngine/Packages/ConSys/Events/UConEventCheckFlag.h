#pragma once

#include "UConEvent.h"

class UConFlagRef;

class UConEventCheckFlag : public UConEvent
{
public:
	using UConEvent::UConEvent;

	UConFlagRef*& flagRef() { return Value<UConFlagRef*>(PropOffsets_ConEventCheckFlag.flagRef); }
	ScriptString& setLabel() { return Value<ScriptString>(PropOffsets_ConEventCheckFlag.setLabel); }
};
