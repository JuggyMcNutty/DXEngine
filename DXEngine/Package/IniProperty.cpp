
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

template<>
float IniPropertyConverter<float>::FromString(const std::string& valueString)
{
	return Convert::to_float(valueString);
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
	// Its first token, in any case, as a class's config reads a bool
	// (UBoolProperty::ImportText, Core.dll 0x10166380): an ini's "False "
	// is false, the blank after it the value's own as the file is read
	std::string value;
	size_t pos = 0;
	if (!UProperty::ReadToken(valueString, pos, value))
		Exception::Throw("Encountered a non-boolean value: " + valueString);
	for (char& c : value)
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
	if (value == "true" || value == "1" || value == "yes")
		return true;
	else if (value == "false" || value == "0" || value == "no")
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

template<>
AudioFrequency IniPropertyConverter<AudioFrequency>::FromString(const std::string& valueString)
{
	if (auto pos = valueString.find("Hz"); pos != std::string::npos)
	{
		int frequency = Convert::to_int32(valueString.substr(0, pos));
		return AudioFrequency(frequency);
	}
	Exception::Throw("Invalid value '" + valueString + "' for changing frequency. The value must be a number that ends with 'Hz'.");
}

template<>
std::string IniPropertyConverter<AudioFrequency>::ToString(const AudioFrequency& value)
{
	return std::to_string(value.frequency) + "Hz";
}

template<>
AudioFrequency IniPropertyConverter<AudioFrequency>::FromIniFile(const IniFile& iniFile, const NameString& section, const NameString& keyName, const AudioFrequency& default_value)
{
	// A value that is no "<number>Hz" (a hand-edited OutputRate=44100) keeps
	// the default, as the other types' rule above has it, rather than
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
