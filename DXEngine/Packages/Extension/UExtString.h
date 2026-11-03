#pragma once

#include "Packages/Core/UObject.h"

class UDXExtString : public UObject
{
public:
	using UObject::UObject;

	void Load(ObjectStream* stream) override;

	ScriptString& Text() { return Value<ScriptString>(PropOffsets_ExtString.Text); }
	int& SpeechPage() { return Value<int>(PropOffsets_ExtString.speechPage); }

	void AppendText(const std::string& newText);
	int GetFirstTextPart(std::string& outText);
	int GetNextTextPart(std::string& outText);
	ScriptString& GetText();
	int GetTextLength();
	int GetTextPart(int startPos, int count, std::string& outText);
	void SetText(const std::string& newText);
};
