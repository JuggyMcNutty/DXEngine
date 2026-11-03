#pragma once

#include "UConObject.h"

class UConSpeech : public UConObject
{
public:
	using UConObject::UConObject;

	ScriptString& Speech() { return Value<ScriptString>(PropOffsets_ConSpeech.Speech); }
	int& soundID() { return Value<int>(PropOffsets_ConSpeech.soundID); }
};
