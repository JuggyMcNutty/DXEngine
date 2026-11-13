#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Utils/Array.h"

// A config property one of the original's C++ classes makes in its
// StaticConstructor, which no package of the game's holds, as the Advanced
// Options show it and GET and SET take it: its name, its type and its
// category (bugs.md D.81). DXEngine's own objects stand in for some of these
// classes -- the client, the audio and render devices --, whose values are
// theirs; the others' are their ini sections'.
struct NativeConfigProperty
{
	enum class Type
	{
		Bool,
		Byte,         // a number 0 to 255, its slider
		Enum,         // EnumNames, 0 up
		Int,
		Float,
		String,
		Class,        // a class's path: MetaClass names its kind
		StringArray,  // a dynamic array of strings, one ini line an element
		NameArray,    // a dynamic array of names
		Aliases,      // UInput's Aliases[40], each (Command=,Alias=)
		RawKeys       // UInput's key bindings: a string property a key
	};

	const char* Name;
	Type PropType;
	const char* Category;
	const char* MetaClass = nullptr;
	const char* const* EnumNames = nullptr;
	int EnumCount = 0;
};

struct NativeConfigClass
{
	const char* Name;        // Package.Class, its config's section
	const char* Super;       // the C++ class it extends, whose properties it has too
	const char* ConfigFile;  // "system" or "user"
	std::vector<NativeConfigProperty> Properties;
};

// The C++ class of that path, in any case; null for any other
const NativeConfigClass* FindNativeConfigClass(const std::string& name);
// The class by its path, or by its name alone in any package, as the
// original's console finds a class (WindowsClient, TcpNetDriver)
const NativeConfigClass* FindNativeConfigClassAnyPackage(const std::string& name);

// Its config properties and its supers', the supers' first
std::vector<const NativeConfigProperty*> AllNativeConfigProperties(const NativeConfigClass* cls);

// Whether the class is that one or extends it
bool NativeConfigClassIsA(const NativeConfigClass* cls, const std::string& name);

// The property of that name, the class's or a super's, any case -- a key's
// of Engine.Input's RawKeys --; null for none
const NativeConfigProperty* FindNativeConfigProperty(const NativeConfigClass* cls, const std::string& name);

// A scalar's text as the class's defaults hold it: the engine's object that
// stands in for the class (its value as GET answers it), else the class's
// section's key imported by its type and written again, the type's empty
// value for a key the section has not. name is the property's, a raw key's
std::string NativeConfigValue(const NativeConfigClass* cls, const NativeConfigProperty& prop, const std::string& name);

// A scalar set as the original's GlobalSetProperty sets it (Core.dll
// 0x101523a0): the engine's object of the class as SET sets it, saved; else
// the text imported against the value as it was (a refused text keeping it)
// into the section, the section written whole, the config flushed; a raw
// key's binding the live input's at once
void SetNativeConfigValue(const NativeConfigClass* cls, const NativeConfigProperty& prop, const std::string& name, const std::string& text);

// A dynamic array's elements as the class's defaults hold them -- an array
// button's change in memory, else its section's lines -- and a change made
// to them: Engine.GameEngine's to the running game engine's list too where
// immediate, as the original's edit changes its engine's in memory
// (Engine::EditGameEngineList); an edit's (save) the section written whole,
// an array button's in memory alone, as the original's buttons write nothing
// (OBSERVED), until the class's config is next saved
Array<std::string> NativeConfigArray(const NativeConfigClass* cls, const NativeConfigProperty& prop);
void ChangeNativeConfigArray(const NativeConfigClass* cls, const NativeConfigProperty& prop, bool immediate, bool save, const std::function<void(Array<std::string>&)>& change);

// UInput's Aliases[index]: its command and alias, its text as the struct's
// export writes it, (Command="...",Alias=...), and an alias set, the section
// written whole and the live input's aliases read again
struct NativeAlias
{
	std::string Command;
	std::string Alias = "None";
};
NativeAlias ReadNativeAlias(int index);
std::string NativeAliasText(const NativeAlias& alias);
void SetNativeAlias(int index, const NativeAlias& alias);

// UInput's key properties: a name of Actor's EInputKey but None's, its IK_
// taken off, as the name table spells it; IK_None's StrProperty0
std::vector<std::string> NativeRawKeyNames();

// The class's section written whole, each property as its type writes it
// (SaveConfig), the config flushed
void SaveNativeConfig(const NativeConfigClass* cls);

// The original's ResetConfig of the class (Core.dll 0x10151ac0): its section
// copied from Default.ini or DefUser.ini, the engine's object taking the
// values, the input's bindings read again; nothing flushed
void ResetNativeConfig(const NativeConfigClass* cls);

// GET and SET of one of these classes by its name (ObjectExec), as the
// original's of the class's defaults (UObject::StaticExec, Core.dll
// 0x10153110): a scalar's text; a static array's first element; a dynamic
// array's elements, (a,b); false for a name the class has not
bool GetNativeConfig(const NativeConfigClass* cls, const std::string& name, std::string& text);
bool SetNativeConfig(const NativeConfigClass* cls, const std::string& name, const std::string& text);
