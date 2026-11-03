#pragma once

#include "Packages/Core/UObject.h"

class NExtString
{
public:
	static void RegisterFunctions();

	static void AppendText(UObject* Self, const ScriptString& NewText);
	static void GetFirstTextPart(UObject* Self, ScriptString& OutText, int& ReturnValue);
	static void GetNextTextPart(UObject* Self, ScriptString& OutText, int& ReturnValue);
	static void GetText(UObject* Self, ScriptString& ReturnValue);
	static void GetTextLength(UObject* Self, int& ReturnValue);
	static void GetTextPart(UObject* Self, int startPos, int Count, ScriptString& OutText, int& ReturnValue);
	static void SetText(UObject* Self, const ScriptString& NewText);
};
