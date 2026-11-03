#pragma once

#include "Packages/ConSys/UConObject.h"

class UConHistoryEvent;

class UConHistory : public UConObject
{
public:
	using UConObject::UConObject;

	UConHistory*& Next() { return Value<UConHistory*>(PropOffsets_ConHistory.Next); }
	BitfieldBool bInfoLink() { return BoolValue(PropOffsets_ConHistory.bInfoLink); }
	ScriptString& conOwnerName() { return Value<ScriptString>(PropOffsets_ConHistory.conOwnerName); }
	UConHistoryEvent*& firstEvent() { return Value<UConHistoryEvent*>(PropOffsets_ConHistory.firstEvent); }
	UConHistoryEvent*& lastEvent() { return Value<UConHistoryEvent*>(PropOffsets_ConHistory.lastEvent); }
	ScriptString& strDescription() { return Value<ScriptString>(PropOffsets_ConHistory.strDescription); }
	ScriptString& strLocation() { return Value<ScriptString>(PropOffsets_ConHistory.strLocation); }
};
