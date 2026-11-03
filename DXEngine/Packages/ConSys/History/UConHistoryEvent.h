#pragma once

#include "Packages/ConSys/UConObject.h"

class UConHistoryEvent : public UConObject
{
public:
	using UConObject::UConObject;

	UConHistoryEvent*& Next() { return Value<UConHistoryEvent*>(PropOffsets_ConHistoryEvent.Next); }
	ScriptString& Speech() { return Value<ScriptString>(PropOffsets_ConHistoryEvent.Speech); }
	ScriptString& conSpeaker() { return Value<ScriptString>(PropOffsets_ConHistoryEvent.conSpeaker); }
	int& soundID() { return Value<int>(PropOffsets_ConHistoryEvent.soundID); }
};
