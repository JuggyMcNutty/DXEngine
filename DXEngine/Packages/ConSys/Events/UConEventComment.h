#pragma once

#include "UConEvent.h"

class UConEventComment : public UConEvent
{
public:
	using UConEvent::UConEvent;

	ScriptString& commentText() { return Value<ScriptString>(PropOffsets_ConEventComment.commentText); }
};
