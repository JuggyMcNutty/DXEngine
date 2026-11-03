#pragma once

#include "UConObject.h"

class UConFlagRef;

class UConChoice : public UConObject
{
public:
	using UConObject::UConObject;

	BitfieldBool bDisplayAsSpeech() { return BoolValue(PropOffsets_ConChoice.bDisplayAsSpeech); }
	ScriptString& choiceLabel() { return Value<ScriptString>(PropOffsets_ConChoice.choiceLabel); }
	ScriptString& choiceText() { return Value<ScriptString>(PropOffsets_ConChoice.choiceText); }
	UConFlagRef*& flagRef() { return Value<UConFlagRef*>(PropOffsets_ConChoice.flagRef); }
	UConChoice*& nextChoice() { return Value<UConChoice*>(PropOffsets_ConChoice.nextChoice); }
	int& skillLevelNeeded() { return Value<int>(PropOffsets_ConChoice.skillLevelNeeded); }
	UClass*& skillNeeded() { return Value<UClass*>(PropOffsets_ConChoice.skillNeeded); }
	int& soundID() { return Value<int>(PropOffsets_ConChoice.soundID); }
};
