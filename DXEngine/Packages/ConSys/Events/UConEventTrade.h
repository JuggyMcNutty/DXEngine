#pragma once

#include "UConEvent.h"

class UActor;

class UConEventTrade : public UConEvent
{
public:
	using UConEvent::UConEvent;

	UActor*& eventOwner() { return Value<UActor*>(PropOffsets_ConEventTrade.eventOwner); }
	ScriptString& eventOwnerName() { return Value<ScriptString>(PropOffsets_ConEventTrade.eventOwnerName); }
};
