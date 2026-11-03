#pragma once

#include "Packages/Core/UObject.h"

class UDXExtString : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;

	ScriptString& Text() { return Value<ScriptString>(PropOffsets_ExtString.Text); }
	int& SpeechPage() { return Value<int>(PropOffsets_ExtString.speechPage); }

	void AppendText(const ScriptString& newText);
	int GetFirstTextPart(ScriptString& outText);
	int GetNextTextPart(ScriptString& outText);
	ScriptString& GetText();
	int GetTextLength();
	int GetTextPart(int startPos, int count, ScriptString& outText);
	void SetText(const ScriptString& newText);
};
