#include "Precomp.h"
#include "NativeConfigClasses.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Package/IniProperty.h"
#include "Packages/Core/UEnum.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Utils/StrTools.h"
#include <cstring>

using T = NativeConfigProperty::Type;

// UClient's enum Details (Engine.dll 0x10375010)
static const char* const DetailNames[] = { "High", "Medium", "Low", "Ultra Low" };
// Galaxy's enum OutputRates (Galaxy.dll 0x10604bb0)
static const char* const OutputRateNames[] = { "8000Hz", "11025Hz", "16000Hz", "22050Hz", "32000Hz", "44100Hz", "48000Hz" };

// Each class's config properties as its StaticConstructor makes them, every
// one CPF_Config alone, in the category it gives (the Advanced Options list
// a class's by name, so their order here is the constructor's alone)
static const std::vector<NativeConfigClass> Classes =
{
	// USystem::StaticConstructor (Core.dll 0x1016b500)
	{ "Core.System", nullptr, "system", {
		{ "PurgeCacheDays", T::Int, "Options" },
		{ "SavePath", T::String, "Options" },
		{ "CachePath", T::String, "Options" },
		{ "CacheExt", T::String, "Options" },
		{ "Paths", T::StringArray, "Options" },
		{ "Suppress", T::NameArray, "Options" },
	} },
	// Engine.u's Engine.uc, the C++ class's config members: GameRenderDevice
	// in no category, as Deus Ex declares it
	{ "Engine.Engine", nullptr, "system", {
		{ "GameRenderDevice", T::Class, "None", "Engine.RenderDevice" },
		{ "AudioDevice", T::Class, "Drivers", "Engine.AudioSubsystem" },
		{ "Console", T::Class, "Drivers", "Engine.Console" },
		{ "NetworkDevice", T::Class, "Drivers", "Engine.NetDriver" },
		{ "Language", T::Class, "Drivers", "Core.Language" },
		{ "CacheSizeMegs", T::Int, "Settings" },
		{ "UseSound", T::Bool, "Settings" },
	} },
	// UGameEngine::StaticConstructor (Engine.dll 0x10388fc0)
	{ "Engine.GameEngine", "Engine.Engine", "system", {
		{ "ServerActors", T::StringArray, "Settings" },
		{ "ServerPackages", T::StringArray, "Settings" },
	} },
	// Deus Ex's game engine (DeusEx.dll's C++ class, no script), whose
	// section the game's ini has
	{ "DeusEx.DeusExGameEngine", "Engine.GameEngine", "system", {} },
	// UInput::StaticInitInput (Engine.dll 0x103930c0): Aliases[40] of the
	// struct Alias, and a string property a key of Actor's EInputKey
	{ "Engine.Input", nullptr, "user", {
		{ "Aliases", T::Aliases, "Aliases" },
		{ "RawKeys", T::RawKeys, "RawKeys" },
	} },
	// UClient::StaticConstructor (Engine.dll 0x10375010)
	{ "Engine.Client", nullptr, "system", {
		{ "WindowedViewportX", T::Int, "Client" },
		{ "WindowedViewportY", T::Int, "Client" },
		{ "WindowedColorBits", T::Int, "Client" },
		{ "FullscreenViewportX", T::Int, "Client" },
		{ "FullscreenViewportY", T::Int, "Client" },
		{ "FullscreenColorBits", T::Int, "Client" },
		{ "MipFactor", T::Float, "Client" },
		{ "Brightness", T::Float, "Display" },
		{ "CaptureMouse", T::Bool, "Display" },
		{ "CurvedSurfaces", T::Bool, "Display" },
		{ "TextureDetail", T::Enum, "Display", nullptr, DetailNames, 4 },
		{ "SkinDetail", T::Enum, "Display", nullptr, DetailNames, 4 },
		{ "ScreenFlashes", T::Bool, "Display" },
		{ "NoLighting", T::Bool, "Display" },
		{ "MinDesiredFrameRate", T::Float, "Display" },
		{ "Decals", T::Bool, "Display" },
		{ "NoDynamicLights", T::Bool, "Display" },
	} },
	// UWindowsClient::StaticConstructor (WinDrv.dll 0x111015f0)
	{ "WinDrv.WindowsClient", "Engine.Client", "system", {
		{ "UseDirectDraw", T::Bool, "Display" },
		{ "UseDirectInput", T::Bool, "Display" },
		{ "UseJoystick", T::Bool, "Display" },
		{ "StartupFullscreen", T::Bool, "Display" },
		{ "SlowVideoBuffering", T::Bool, "Display" },
		{ "DeadZoneXYZ", T::Bool, "Joystick" },
		{ "DeadZoneRUV", T::Bool, "Joystick" },
		{ "InvertVertical", T::Bool, "Joystick" },
		{ "ScaleXYZ", T::Float, "Joystick" },
		{ "ScaleRUV", T::Float, "Joystick" },
	} },
	// URenderDevice::StaticConstructor (Engine.dll 0x103713c0)
	{ "Engine.RenderDevice", nullptr, "system", {
		{ "VolumetricLighting", T::Bool, "Client" },
		{ "ShinySurfaces", T::Bool, "Client" },
		{ "Coronas", T::Bool, "Client" },
		{ "HighDetailActors", T::Bool, "Client" },
		{ "Description", T::String, "Client" },
		{ "DescFlags", T::Int, "Client" },
		{ "DetailTextures", T::Bool, "Client" },
	} },
	// UD3DRenderDevice::StaticConstructor (D3DDrv.dll 0x10001880)
	{ "D3DDrv.D3DRenderDevice", "Engine.RenderDevice", "system", {
		{ "UseMipmapping", T::Bool, "Options" },
		{ "UseTrilinear", T::Bool, "Options" },
		{ "UseMultitexture", T::Bool, "Options" },
		{ "UsePalettes", T::Bool, "Options" },
		{ "UseGammaCorrection", T::Bool, "Options" },
		{ "Use3dfx", T::Bool, "Options" },
		{ "UseTripleBuffering", T::Bool, "Options" },
		{ "UseVSync", T::Bool, "Options" },
		{ "UsePrecache", T::Bool, "Options" },
		{ "UseVideoMemoryVB", T::Bool, "Options" },
		{ "UseAGPTextures", T::Bool, "Options" },
		{ "UseVertexFog", T::Bool, "Options" },
		{ "dwVendorId", T::Int, "Options" },
		{ "dwDeviceId", T::Int, "Options" },
	} },
	// OpenGLDrv's class makes none of its own: it reads its settings from
	// the config itself (UDXEngineRenderDevice::ReadSection)
	{ "OpenGLDrv.OpenGLRenderDevice", "Engine.RenderDevice", "system", {} },
	// SoftDrv's (UDXEngineRenderDevice::ReadSection); their category none of
	// the pages asks for
	{ "SoftDrv.SoftwareRenderDevice", "Engine.RenderDevice", "system", {
		{ "HighResTextureSmooth", T::Bool, "Options" },
		{ "LowResTextureSmooth", T::Bool, "Options" },
		{ "FastTranslucency", T::Bool, "Options" },
		{ "DetailBias", T::Float, "Options" },
	} },
	// UNetDriver::StaticConstructor (Engine.dll 0x10409080)
	{ "Engine.NetDriver", nullptr, "system", {
		{ "ConnectionTimeout", T::Float, "Client" },
		{ "InitialConnectTimeout", T::Float, "Client" },
		{ "KeepAliveTime", T::Float, "Client" },
		{ "RelevantTimeout", T::Float, "Client" },
		{ "SpawnPrioritySeconds", T::Float, "Client" },
		{ "ServerTravelPause", T::Float, "Client" },
		{ "MaxClientRate", T::Int, "Client" },
		{ "NetServerMaxTickRate", T::Int, "Client" },
		{ "LanServerMaxTickRate", T::Int, "Client" },
		{ "DynamicUpdateRate", T::Int, "Client" },
		{ "StaticUpdateRate", T::Int, "Client" },
		{ "AllowDownloads", T::Bool, "Client" },
	} },
	// UTcpNetDriver's class takes UNetDriver's constructor (IpDrv.dll
	// 0x1070b740)
	{ "IpDrv.TcpNetDriver", "Engine.NetDriver", "system", {} },
	// UGalaxyAudioSubsystem::StaticConstructor (Galaxy.dll 0x10604bb0)
	{ "Galaxy.GalaxyAudioSubsystem", nullptr, "system", {
		{ "UseDirectSound", T::Bool, "Audio" },
		{ "UseFilter", T::Bool, "Audio" },
		{ "UseSurround", T::Bool, "Audio" },
		{ "UseStereo", T::Bool, "Audio" },
		{ "UseCDMusic", T::Bool, "Audio" },
		{ "UseDigitalMusic", T::Bool, "Audio" },
		{ "UseReverb", T::Bool, "Audio" },
		{ "Use3dHardware", T::Bool, "Audio" },
		{ "ReverseStereo", T::Bool, "Audio" },
		{ "LowSoundQuality", T::Bool, "Audio" },
		{ "Latency", T::Int, "Audio" },
		{ "OutputRate", T::Enum, "Audio", nullptr, OutputRateNames, 7 },
		{ "EffectsChannels", T::Int, "Audio" },
		{ "MusicVolume", T::Byte, "Audio" },
		{ "SoundVolume", T::Byte, "Audio" },
		{ "SpeechVolume", T::Byte, "Audio" },
		{ "AmbientFactor", T::Float, "Audio" },
		{ "DopplerSpeed", T::Float, "Audio" },
	} },
};

const NativeConfigClass* FindNativeConfigClass(const std::string& name)
{
	for (const NativeConfigClass& cls : Classes)
	{
		if (StrTools::equals_ignore_case(name, cls.Name))
			return &cls;
	}
	return nullptr;
}

std::vector<const NativeConfigProperty*> AllNativeConfigProperties(const NativeConfigClass* cls)
{
	std::vector<const NativeConfigProperty*> result;
	if (!cls)
		return result;
	if (cls->Super)
		result = AllNativeConfigProperties(FindNativeConfigClass(cls->Super));
	for (const NativeConfigProperty& prop : cls->Properties)
		result.push_back(&prop);
	return result;
}

bool NativeConfigClassIsA(const NativeConfigClass* cls, const std::string& name)
{
	for (; cls; cls = cls->Super ? FindNativeConfigClass(cls->Super) : nullptr)
	{
		if (StrTools::equals_ignore_case(name, cls->Name))
			return true;
	}
	return false;
}

const NativeConfigClass* FindNativeConfigClassAnyPackage(const std::string& name)
{
	if (const NativeConfigClass* cls = FindNativeConfigClass(name))
		return cls;
	if (name.find('.') != std::string::npos)
		return nullptr;
	for (const NativeConfigClass& cls : Classes)
	{
		const char* dot = std::strchr(cls.Name, '.');
		if (dot && StrTools::equals_ignore_case(name, dot + 1))
			return &cls;
	}
	return nullptr;
}

static bool Same(const std::string& a, const std::string& b)
{
	return StrTools::equals_ignore_case(a, b);
}

static NameString ConfigFile(const NativeConfigClass* cls)
{
	return NameString(cls->ConfigFile);
}

std::vector<std::string> NativeRawKeyNames()
{
	std::vector<std::string> names;
	UEnum* keys = nullptr;
	try
	{
		keys = UObject::TryCast<UEnum>(engine->packages->GetPackage("Engine")->GetUObject("Enum", "EInputKey", "Actor", false));
	}
	catch (...)
	{
	}
	if (keys)
	{
		for (size_t i = 0; i < keys->ElementNames.size() && i < 255; i++)
		{
			std::string name = keys->ElementNames[i].ToString();
			if (name.empty() || keys->ElementNames[i].IsNone())
				continue;
			if (name.size() > 3 && Same(name.substr(0, 3), "IK_"))
				name = name.substr(3);
			if (Same(name, "None"))
				name = "StrProperty0";
			names.push_back(NameString(name).ToString());
		}
		return names;
	}
	for (int i = 0; i < 255; i++)
		names.push_back(i == 0 ? std::string("StrProperty0") : std::string(Engine::keynames[i]));
	return names;
}

const NativeConfigProperty* FindNativeConfigProperty(const NativeConfigClass* cls, const std::string& name)
{
	const NativeConfigProperty* rawKeys = nullptr;
	for (const NativeConfigProperty* prop : AllNativeConfigProperties(cls))
	{
		if (Same(name, prop->Name) && prop->PropType != T::RawKeys)
			return prop;
		if (prop->PropType == T::RawKeys)
			rawKeys = prop;
	}
	if (rawKeys)
	{
		for (const std::string& key : NativeRawKeyNames())
		{
			if (Same(key, name))
				return rawKeys;
		}
	}
	return nullptr;
}

// A text read into the property's type and written out again, as the
// original's ImportText and ExportText of that type take it; current is the
// value as it was, kept where the type refuses the text
static std::string ImportNative(const NativeConfigProperty& prop, const std::string& text, const std::string& current)
{
	switch (prop.PropType)
	{
	case T::Bool:
	{
		// UBoolProperty::ImportText (Core.dll 0x10166380): its first token
		size_t pos = 0;
		std::string token;
		if (UProperty::ReadToken(text, pos, token))
		{
			if (token == "1" || Same(token, "True"))
				return "True";
			if (token == "0" || Same(token, "False"))
				return "False";
		}
		return current;
	}
	case T::Byte:
		// UByteProperty::ImportText (0x10165aa0): digits at its start, a byte
		if (!text.empty() && text[0] >= '0' && text[0] <= '9')
			return std::to_string((uint8_t)std::atoi(text.c_str()));
		return current;
	case T::Enum:
	{
		uint8_t value = 0;
		for (int i = 0; i < prop.EnumCount; i++)
		{
			if (current == prop.EnumNames[i])
				value = (uint8_t)i;
		}
		ImportEnumText(text, prop.EnumNames, prop.EnumCount, value);
		return prop.EnumNames[std::min<int>(value, prop.EnumCount - 1)];
	}
	case T::Int:
		// UIntProperty::ImportText (0x10165f00): a '-' or a digit first
		if (!text.empty() && (text[0] == '-' || (text[0] >= '0' && text[0] <= '9')))
			return std::to_string(std::atoi(text.c_str()));
		return current;
	case T::Float:
		// appAtof, never refused: "1500" is 1500.000000
		return StrTools::float_text((float)std::strtod(text.c_str(), nullptr));
	case T::Class:
	{
		// A class's path, bare or as an object's text, Class'Galaxy.X'
		std::string path = text;
		size_t quote = path.find('\'');
		if (quote != std::string::npos && path.size() > quote + 1 && path.back() == '\'')
			path = path.substr(quote + 1, path.size() - quote - 2);
		return path.empty() ? std::string("None") : path;
	}
	default:
		return text;
	}
}

static std::string NativeDefault(const NativeConfigProperty& prop)
{
	switch (prop.PropType)
	{
	case T::Bool: return "False";
	case T::Byte:
	case T::Int: return "0";
	case T::Float: return StrTools::float_text(0.0);
	case T::Enum: return prop.EnumCount > 0 ? prop.EnumNames[0] : "";
	case T::Class: return "None";
	default: return {};
	}
}

// A property's text in the class's section, as its type reads and writes
// it, its type's empty value where the section has no key: Engine.GameEngine,
// whose section Deus Ex has not, shows CacheSizeMegs 0 and UseSound False
static std::string NativeIniText(const NativeConfigClass* cls, const NativeConfigProperty& prop, const std::string& key)
{
	std::string text;
	if (!engine->packages->FindIniValue(ConfigFile(cls), cls->Name, key, text))
		return NativeDefault(prop);
	return ImportNative(prop, text, NativeDefault(prop));
}

std::string NativeConfigValue(const NativeConfigClass* cls, const NativeConfigProperty& prop, const std::string& name)
{
	if (prop.PropType == T::RawKeys)
		return engine->packages->GetIniValue(ConfigFile(cls), cls->Name, name);
	if (UObject* live = engine->LiveSubsystem(cls->Name))
	{
		bool known = false;
		std::string text = engine->GetLiveSubsystemProperty(live, name, known);
		if (known)
			return text;
	}
	return NativeIniText(cls, prop, name);
}

void SetNativeConfigValue(const NativeConfigClass* cls, const NativeConfigProperty& prop, const std::string& name, const std::string& text)
{
	if (prop.PropType == T::RawKeys)
	{
		// The live input's binding at once, as SET Input sets it
		engine->keybindings[name] = text;
		engine->packages->SetIniValue(ConfigFile(cls), cls->Name, name, text);
		SaveNativeConfig(cls);
		return;
	}
	if (UObject* live = engine->LiveSubsystem(cls->Name))
	{
		if (engine->SetLiveSubsystemProperty(live, name, text))
			return;
	}
	std::string current = NativeIniText(cls, prop, name);
	engine->packages->SetIniValue(ConfigFile(cls), cls->Name, name, ImportNative(prop, text, current));
	SaveNativeConfig(cls);
}

// The C++ classes' dynamic arrays an array button changed, by class and
// property: their defaults in memory, which a save of the class's config
// writes and a reset drops
static std::map<std::string, Array<std::string>>& ArrayDefaults()
{
	static std::map<std::string, Array<std::string>> defaults;
	return defaults;
}

static std::string ArrayKey(const NativeConfigClass* cls, const NativeConfigProperty& prop)
{
	return NameString::Folded(std::string(cls->Name) + "." + prop.Name);
}

static void DropArrayDefaults(const NativeConfigClass* cls)
{
	for (const NativeConfigProperty* prop : AllNativeConfigProperties(cls))
		ArrayDefaults().erase(ArrayKey(cls, *prop));
}

Array<std::string> NativeConfigArray(const NativeConfigClass* cls, const NativeConfigProperty& prop)
{
	auto it = ArrayDefaults().find(ArrayKey(cls, prop));
	if (it != ArrayDefaults().end())
		return it->second;
	return engine->packages->GetIniValues(ConfigFile(cls), cls->Name, prop.Name);
}

void ChangeNativeConfigArray(const NativeConfigClass* cls, const NativeConfigProperty& prop, bool immediate, bool save, const std::function<void(Array<std::string>&)>& change)
{
	// The running game engine's list first, as GlobalSetProperty sets the
	// objects before the defaults (as ruled: the original's edit of
	// Engine.GameEngine's arrays changes its engine's in memory, an element
	// past a list's end in neither), then the class's
	if (immediate && NativeConfigClassIsA(cls, "Engine.GameEngine"))
		change(engine->EditGameEngineList(prop.Name));
	Array<std::string> values = NativeConfigArray(cls, prop);
	change(values);
	ArrayDefaults()[ArrayKey(cls, prop)] = values;
	if (save)
		SaveNativeConfig(cls);
}

NativeAlias ReadNativeAlias(int index)
{
	NativeAlias alias;
	std::string text;
	if (!engine->packages->FindIniValue("user", "Engine.Input", "Aliases[" + std::to_string(index) + "]", text))
		return alias;
	Array<UProperty::TextPair> pairs;
	UProperty::ParseStructText(text, pairs);
	for (const UProperty::TextPair& pair : pairs)
	{
		if (pair.Key == NameString("Command"))
			alias.Command = UProperty::Unquote(pair.Value);
		else if (pair.Key == NameString("Alias"))
			alias.Alias = UProperty::Unquote(pair.Value).empty() ? std::string("None") : UProperty::Unquote(pair.Value);
	}
	return alias;
}

std::string NativeAliasText(const NativeAlias& alias)
{
	return "(Command=\"" + alias.Command + "\",Alias=" + (alias.Alias.empty() ? std::string("None") : alias.Alias) + ")";
}

void SetNativeAlias(int index, const NativeAlias& alias)
{
	NativeAlias value = alias;
	if (value.Alias.empty())
		value.Alias = "None";
	else
		value.Alias = NameString(value.Alias).ToString();
	engine->packages->SetIniValue("user", "Engine.Input", "Aliases[" + std::to_string(index) + "]", NativeAliasText(value));
	SaveNativeConfig(FindNativeConfigClass("Engine.Input"));
	engine->inputAliases.clear();
	engine->LoadKeybindings();
}

void SaveNativeConfig(const NativeConfigClass* cls)
{
	if (!cls)
		return;
	PackageManager* pm = engine->packages.get();
	NameString file = ConfigFile(cls);
	for (const NativeConfigProperty* prop : AllNativeConfigProperties(cls))
	{
		switch (prop->PropType)
		{
		case T::StringArray:
		case T::NameArray:
			pm->SetIniValues(file, cls->Name, prop->Name, NativeConfigArray(cls, *prop));
			break;
		case T::Aliases:
			for (int i = 0; i < 40; i++)
				pm->SetIniValue(file, cls->Name, "Aliases[" + std::to_string(i) + "]", NativeAliasText(ReadNativeAlias(i)));
			break;
		case T::RawKeys:
			for (const std::string& key : NativeRawKeyNames())
				pm->SetIniValue(file, cls->Name, key, pm->GetIniValue(file, cls->Name, key));
			break;
		default:
			pm->SetIniValue(file, cls->Name, prop->Name, NativeIniText(cls, *prop, prop->Name));
			break;
		}
	}
	// Its arrays in the section now, as the defaults hold them
	DropArrayDefaults(cls);
	engine->FlushConfig();
}

void ResetNativeConfig(const NativeConfigClass* cls)
{
	NameString file = ConfigFile(cls);
	engine->packages->ResetIniSection(file, cls->Name);
	DropArrayDefaults(cls);
	if (UObject* live = engine->LiveSubsystem(cls->Name))
	{
		for (const auto& [key, value] : engine->packages->GetDefaultIniKeyValues(file, cls->Name))
			engine->SetLiveSubsystemProperty(live, key, value, false);
	}
	if (NativeConfigClassIsA(cls, "Engine.Input"))
	{
		engine->inputAliases.clear();
		engine->LoadKeybindings();
	}
	// The running game engine's lists read again from its class's section,
	// as ResetConfig loads every object of the class's config again
	if (NativeConfigClassIsA(cls, "Engine.GameEngine"))
		engine->GameEngineLists.clear();
}

// A dynamic array's text as GET writes it, (a,b): a string quoted, a name bare
static std::string ArrayText(const NativeConfigProperty& prop, const Array<std::string>& values)
{
	std::string text = "(";
	for (size_t i = 0; i < values.size(); i++)
	{
		if (i > 0)
			text += ",";
		text += prop.PropType == T::StringArray ? "\"" + values[i] + "\"" : values[i];
	}
	return text + ")";
}

// (a,b,...) as an array's import reads it: each element to the next comma
// outside quotes, a quoted one unquoted; false for text that is no list
static bool ParseArrayText(const std::string& text, Array<std::string>& values)
{
	size_t pos = text.find_first_not_of(' ');
	if (pos == std::string::npos || text[pos] != '(')
		return false;
	values.clear();
	std::string element;
	bool quoted = false, any = false;
	for (pos++; pos < text.size(); pos++)
	{
		char c = text[pos];
		if (c == '"')
			quoted = !quoted;
		if (!quoted && (c == ',' || c == ')'))
		{
			if (any || c == ',' || !element.empty())
				values.push_back(UProperty::Unquote(element));
			element.clear();
			any = true;
			if (c == ')')
				return true;
			continue;
		}
		element += c;
	}
	return false;
}

bool GetNativeConfig(const NativeConfigClass* cls, const std::string& name, std::string& text)
{
	const NativeConfigProperty* prop = FindNativeConfigProperty(cls, name);
	if (!prop)
		return false;
	if (prop->PropType == T::StringArray || prop->PropType == T::NameArray)
		text = ArrayText(*prop, NativeConfigArray(cls, *prop));
	else if (prop->PropType == T::Aliases)
		text = NativeAliasText(ReadNativeAlias(0));
	else if (prop->PropType == T::Class)
	{
		// An object's text, its class and path (Class'Galaxy.X'), or None
		std::string path = NativeConfigValue(cls, *prop, prop->Name);
		text = Same(path, "None") || path.empty() ? std::string("None") : "Class'" + path + "'";
	}
	else
		text = NativeConfigValue(cls, *prop, prop->PropType == T::RawKeys ? name : std::string(prop->Name));
	return true;
}

bool SetNativeConfig(const NativeConfigClass* cls, const std::string& name, const std::string& text)
{
	const NativeConfigProperty* prop = FindNativeConfigProperty(cls, name);
	if (!prop)
		return false;
	if (prop->PropType == T::StringArray || prop->PropType == T::NameArray)
	{
		Array<std::string> values;
		if (ParseArrayText(text, values))
			ChangeNativeConfigArray(cls, *prop, true, true, [&](Array<std::string>& list) { list = values; });
	}
	else if (prop->PropType == T::Aliases)
	{
		// The first element, as SET imports a static array's
		NativeAlias alias = ReadNativeAlias(0);
		Array<UProperty::TextPair> pairs;
		if (UProperty::ParseStructText(text, pairs))
		{
			for (const UProperty::TextPair& pair : pairs)
			{
				if (pair.Key == NameString("Command"))
					alias.Command = UProperty::Unquote(pair.Value);
				else if (pair.Key == NameString("Alias"))
					alias.Alias = UProperty::Unquote(pair.Value);
			}
			SetNativeAlias(0, alias);
		}
	}
	else
	{
		SetNativeConfigValue(cls, *prop, prop->PropType == T::RawKeys ? name : std::string(prop->Name), text);
	}
	return true;
}
