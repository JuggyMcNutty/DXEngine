#pragma once

#include "UConEvent.h"

class UConEventCheckObject : public UConEvent
{
public:
	using UConEvent::UConEvent;

	ScriptString& ObjectName() { return Value<ScriptString>(PropOffsets_ConEventCheckObject.ObjectName); }
	UClass*& checkObject() { return Value<UClass*>(PropOffsets_ConEventCheckObject.checkObject); }
	ScriptString& failLabel() { return Value<ScriptString>(PropOffsets_ConEventCheckObject.failLabel); }
};
