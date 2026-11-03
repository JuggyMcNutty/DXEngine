#pragma once

#include "UConEvent.h"

class UActor;

class UConEventTransferObject : public UConEvent
{
public:
	using UConEvent::UConEvent;

	ScriptString& ObjectName() { return Value<ScriptString>(PropOffsets_ConEventTransferObject.ObjectName); }
	ScriptString& failLabel() { return Value<ScriptString>(PropOffsets_ConEventTransferObject.failLabel); }
	UActor*& fromActor() { return Value<UActor*>(PropOffsets_ConEventTransferObject.fromActor); }
	ScriptString& fromName() { return Value<ScriptString>(PropOffsets_ConEventTransferObject.fromName); }
	UClass*& giveObject() { return Value<UClass*>(PropOffsets_ConEventTransferObject.giveObject); }
	UActor*& toActor() { return Value<UActor*>(PropOffsets_ConEventTransferObject.toActor); }
	ScriptString& toName() { return Value<ScriptString>(PropOffsets_ConEventTransferObject.toName); }
	int& transferCount() { return Value<int>(PropOffsets_ConEventTransferObject.transferCount); }
};
