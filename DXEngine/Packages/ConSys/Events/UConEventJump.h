#pragma once

#include "UConEvent.h"

class UConEventJump : public UConEvent
{
public:
	using UConEvent::UConEvent;

	int& conID() { return Value<int>(PropOffsets_ConEventJump.conID); }
	UConversation*& jumpCon() { return Value<UConversation*>(PropOffsets_ConEventJump.jumpCon); }
	ScriptString& jumpLabel() { return Value<ScriptString>(PropOffsets_ConEventJump.jumpLabel); }
};
