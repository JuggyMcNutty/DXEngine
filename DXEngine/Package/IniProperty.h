#pragma once

#include <string>
#include <vector>
#include <functional>
#include <initializer_list>

#include "IniFile.h"
#include "Packages/Engine/Resources/USound.h"

template <typename T>
class IniPropertyConverter
{
public:
	static std::string ToString(const T& value);
	static T FromString(const std::string& valueString);
	static T FromString(const char* valueString) { return FromString(std::string(valueString)); }
	static T FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const T& default_value);

	IniPropertyConverter() = delete;
};

// An enum byte's text as the original's import reads it
// (UByteProperty::ImportText, Core.dll 0x10165aa0): the text's first token,
// one of the names in any case; false for any other text, a number among
// them, the value then as it was.
bool ImportEnumText(const std::string& text, const char* const* names, int count, uint8_t& value);

// The config properties of an original native class that DXEngine keeps no
// member for (the client's MipFactor, D3DDrv's UseVSync), each a value of its
// type: none until the ini names it, read and SET by the type's import, GET
// and saved by its export, as the original's class holds it. None of them
// changes what DXEngine does (bugs.md ENG.32). A class's list is its
// StaticConstructor's.
class NativeConfigProperties
{
public:
	enum class Type { Bool, Int, Float, String };
	struct Definition { const char* Name; Type PropType; };

	void Define(std::initializer_list<Definition> definitions);
	bool Has(const NameString& name) const;
	std::string Get(const NameString& name) const;
	void Set(const NameString& name, const std::string& text);
	void Load(const IniFile& iniFile, const NameString& section);
	void ForEach(const std::function<void(const std::string& name, const std::string& value)>& callback) const;

private:
	struct Value
	{
		std::string Name;
		Type PropType = Type::Bool;
		bool Bool = false;
		int Int = 0;
		float Float = 0.0f;
		std::string String;
	};
	const Value* Find(const NameString& name) const;
	static std::string Export(const Value& value);
	std::vector<Value> Values;
};
