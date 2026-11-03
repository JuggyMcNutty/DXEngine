#pragma once

#include "UConObject.h"

class UConItem;

class UConversationList : public UConObject
{
public:
	using UConObject::UConObject;

	UConItem*& conversations() { return Value<UConItem*>(PropOffsets_ConversationList.conversations); }
	ScriptString& missionDescription() { return Value<ScriptString>(PropOffsets_ConversationList.missionDescription); }
	int& missionNumber() { return Value<int>(PropOffsets_ConversationList.missionNumber); }
};
