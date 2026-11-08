
#include "Precomp.h"
#include "Utils/StrTools.h"
#include "IniProperty.h"
#include "Utils/Exception.h"
#include "Utils/Convert.h"
#include "Packages/Core/UObject.h"
#include "Packages/Core/Properties/UProperty.h"
#include <cstdlib>
#include <sstream>

// A value that does not read as its type (a hand-edited ini's) is the
// default, as the original's import leaves an int, a byte or a bool as it
// was; it does not stop the engine. A float is read as the original's is.
// An empty value is none (GetValue) but for a float.
template<typename T>
static T FromIniOrDefault(const IniFile& iniFile, const NameString& section, const NameString& keyName, const T& default_value)
{
	try
	{
		return IniPropertyConverter<T>::FromString(iniFile.GetValue(section, keyName, IniPropertyConverter<T>::ToString(default_value)));
	}
	catch (const std::exception&)
	{
		return default_value;
	}
}

template<>
int IniPropertyConverter<int>::FromString(const std::string& valueString)
{
	return Convert::to_int32(valueString);
}

template<>
std::string IniPropertyConverter<int>::ToString(const int& value)
{
	return std::to_string(value);
}

template<>
int IniPropertyConverter<int>::FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const int& default_value)
{
	return FromIniOrDefault<int>(iniFile, section, keyName, default_value);
}

//====================================================================

// A SET's float as the original's import reads one (UFloatProperty's
// ImportText, appAtof): the number the text starts with, 0 for any other
// text, never refused (bugs.md ENG.32)
template<>
float IniPropertyConverter<float>::FromString(const std::string& valueString)
{
	return (float)strtod(valueString.c_str(), nullptr);
}

template<>
std::string IniPropertyConverter<float>::ToString(const float& value)
{
	return StrTools::float_text(value);
}

// A float as the original's import reads one (appAtof), the class config's
// way (UFloatProperty::SetValueFromString): the number the text starts with,
// 0 for an empty value or any other text, the default only for a key that
// is not there.
template<>
float IniPropertyConverter<float>::FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const float& default_value)
{
	std::string value;
	if (!iniFile.FindValue(section, keyName, value))
		return default_value;
	return (float)strtod(value.c_str(), nullptr);
}

//====================================================================

template<>
uint8_t IniPropertyConverter<uint8_t>::FromString(const std::string& valueString)
{
	return Convert::to_uint8(valueString);
}

template<>
std::string IniPropertyConverter<uint8_t>::ToString(const uint8_t& value)
{
	return std::to_string(value);
}

template<>
uint8_t IniPropertyConverter<uint8_t>::FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const uint8_t& default_value)
{
	return FromIniOrDefault<uint8_t>(iniFile, section, keyName, default_value);
}

//====================================================================

template<>
bool IniPropertyConverter<bool>::FromString(const std::string& valueString)
{
	// Its first token as a class's config reads a bool
	// (UBoolProperty::ImportText, Core.dll 0x10166380): 1 or True in any
	// case, 0 or False, so an ini's "False " is false, the blank after it
	// the value's own as the file is read; any other word, Yes and No among
	// them, refused (bugs.md ENG.24), the setting keeping its value
	std::string value;
	size_t pos = 0;
	if (!UProperty::ReadToken(valueString, pos, value))
		Exception::Throw("Encountered a non-boolean value: " + valueString);
	if (value == "1" || StrTools::equals_ignore_case(value, "True"))
		return true;
	else if (value == "0" || StrTools::equals_ignore_case(value, "False"))
		return false;
	else
		Exception::Throw("Encountered a non-boolean value: " + valueString);
}


template<>
std::string IniPropertyConverter<bool>::ToString(const bool& value)
{
	return value ? "True" : "False";
}

template<>
bool IniPropertyConverter<bool>::FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const bool& default_value)
{
	return FromIniOrDefault<bool>(iniFile, section, keyName, default_value);
}

//====================================================================

template<>
std::string IniPropertyConverter<std::string>::FromString(const std::string& valueString)
{
	return valueString;
}

template<>
std::string IniPropertyConverter<std::string>::ToString(const std::string& value)
{
	return value;
}

template<>
std::string IniPropertyConverter<std::string>::FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const std::string& default_value)
{
	return iniFile.GetValue(section, keyName, default_value);
}

//====================================================================

// Galaxy's OutputRate is a byte enum of seven names (its StaticConstructor,
// Galaxy.dll 0x10604be0), read as any enum is (UByteProperty::ImportText,
// Core.dll 0x10165aa0): the text's first token, one of the names in any
// case. Any other text, a number among them, is refused, the rate kept
// (bugs.md D.55).
template<>
AudioFrequency IniPropertyConverter<AudioFrequency>::FromString(const std::string& valueString)
{
	static const int rates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
	std::string token;
	size_t pos = 0;
	if (UProperty::ReadToken(valueString, pos, token))
	{
		for (int rate : rates)
		{
			if (StrTools::equals_ignore_case(token, std::to_string(rate) + "Hz"))
				return AudioFrequency(rate);
		}
	}
	Exception::Throw("Invalid value '" + valueString + "' for the output rate: none of Galaxy's names, 8000Hz, 11025Hz, 16000Hz, 22050Hz, 32000Hz, 44100Hz and 48000Hz");
}

template<>
std::string IniPropertyConverter<AudioFrequency>::ToString(const AudioFrequency& value)
{
	return std::to_string(value.frequency) + "Hz";
}

template<>
AudioFrequency IniPropertyConverter<AudioFrequency>::FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const AudioFrequency& default_value)
{
	// A value that is none of the names (a hand-edited OutputRate=44100)
	// keeps the default, as the other types' rule above has it, rather than
	// stopping the engine at its start
	try
	{
		return FromString(iniFile.GetValue(section, keyName, ToString(default_value)));
	}
	catch (const std::exception&)
	{
		return default_value;
	}
}

//====================================================================

template<>
Color IniPropertyConverter<Color>::FromString(const std::string& valueString)
{
	if (valueString.size() <= 2 || valueString[0] != '(' || valueString[valueString.size() - 1] != ')')
		Exception::Throw("Invalid Color String: " + valueString);

	// Get rid of ()s
	auto subString = valueString.substr(1, valueString.size() - 2);

	uint8_t r = 0, g = 0, b = 0, a = 0;

	std::stringstream ss(subString);

	std::string t;

	while (getline(ss, t, ','))
	{
		size_t equals = t.find('=');

		if (equals == std::string::npos)
			Exception::Throw("Malformed Color String: " + valueString);

		auto lhs = t.substr(0, equals);

		auto rhs = Convert::to_uint8(t.substr(equals + 1));

		if (lhs == "R")
			r = rhs;
		else if (lhs == "G")
			g = rhs;
		else if (lhs == "B")
			b = rhs;
		else if (lhs == "A")
			a = rhs;
		else
			Exception::Throw("Unexpected Color Property: " + lhs);
	}

	return Color {
		.R = r,
		.G = g,
		.B = b,
		.A = a
	};
}

template <>
std::string IniPropertyConverter<Color>::ToString(const Color& value)
{
	return "(R=" + std::to_string(value.R)
		   + ",G=" + std::to_string(value.G)
		   + ",B=" + std::to_string(value.B)
	       + ",A=" + std::to_string(value.A) + ")";
}

template<>
Color IniPropertyConverter<Color>::FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const Color& default_value)
{
	return FromString(iniFile.GetValue(section, keyName, ToString(default_value)));
}

//====================================================================

bool ImportEnumText(const std::string& text, const char* const* names, int count, uint8_t& value)
{
	std::string token;
	size_t pos = 0;
	if (!UProperty::ReadToken(text, pos, token))
		return false;
	for (int i = 0; i < count; i++)
	{
		if (StrTools::equals_ignore_case(token, names[i]))
		{
			value = (uint8_t)i;
			return true;
		}
	}
	return false;
}

//====================================================================

void NativeConfigProperties::Define(std::initializer_list<Definition> definitions)
{
	Values.clear();
	for (const Definition& definition : definitions)
	{
		Value value;
		value.Name = definition.Name;
		value.PropType = definition.PropType;
		Values.push_back(value);
	}
}

const NativeConfigProperties::Value* NativeConfigProperties::Find(const NameString& name) const
{
	for (const Value& value : Values)
	{
		if (name == NameString(value.Name))
			return &value;
	}
	return nullptr;
}

bool NativeConfigProperties::Has(const NameString& name) const
{
	return Find(name) != nullptr;
}

std::string NativeConfigProperties::Export(const Value& value)
{
	switch (value.PropType)
	{
	case Type::Bool: return IniPropertyConverter<bool>::ToString(value.Bool);
	case Type::Int: return IniPropertyConverter<int>::ToString(value.Int);
	case Type::Float: return IniPropertyConverter<float>::ToString(value.Float);
	default: return value.String;
	}
}

std::string NativeConfigProperties::Get(const NameString& name) const
{
	const Value* value = Find(name);
	return value ? Export(*value) : std::string();
}

// A text its type refuses (a bool's "Yes", an int's word) leaves the value
// as it was; a float's never fails
void NativeConfigProperties::Set(const NameString& name, const std::string& text)
{
	Value* value = const_cast<Value*>(Find(name));
	if (!value)
		return;
	try
	{
		switch (value->PropType)
		{
		case Type::Bool: value->Bool = IniPropertyConverter<bool>::FromString(text); break;
		case Type::Int: value->Int = IniPropertyConverter<int>::FromString(text); break;
		case Type::Float: value->Float = IniPropertyConverter<float>::FromString(text); break;
		default: value->String = text; break;
		}
	}
	catch (const std::exception&)
	{
	}
}

void NativeConfigProperties::Load(const IniFile& iniFile, const NameString& section)
{
	for (const Value& value : Values)
	{
		std::string text;
		if (iniFile.FindValue(section, value.Name, text))
			Set(value.Name, text);
	}
}

void NativeConfigProperties::ForEach(const std::function<void(const std::string& name, const std::string& value)>& callback) const
{
	for (const Value& value : Values)
		callback(value.Name, Export(value));
}
