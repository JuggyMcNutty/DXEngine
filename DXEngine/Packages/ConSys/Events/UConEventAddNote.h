#pragma once

#include "UConEvent.h"

class UConEventAddNote : public UConEvent
{
public:
	using UConEvent::UConEvent;

	BitfieldBool bNoteAdded() { return BoolValue(PropOffsets_ConEventAddNote.bNoteAdded); }
	ScriptString& noteText() { return Value<ScriptString>(PropOffsets_ConEventAddNote.noteText); }
};
