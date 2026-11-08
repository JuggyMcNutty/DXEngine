
#include "Precomp.h"
#include "UDXEngineClient.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Package/IniProperty.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Utils/StrTools.h"

// The config properties of the original's UClient (Engine.dll 0x10375010)
// and UWindowsClient (WinDrv.dll 0x111015f0) that DXEngine has no use for,
// kept as the original's class keeps them (bugs.md ENG.32)
void UDXEngineClient::DefineExtra()
{
	using Type = NativeConfigProperties::Type;
	Extra.Define({
		{ "MipFactor", Type::Float },
		{ "CaptureMouse", Type::Bool },
		{ "CurvedSurfaces", Type::Bool },
		{ "UseDirectDraw", Type::Bool },
		{ "SlowVideoBuffering", Type::Bool },
		{ "DeadZoneXYZ", Type::Bool },
		{ "DeadZoneRUV", Type::Bool },
		{ "InvertVertical", Type::Bool },
		{ "ScaleXYZ", Type::Float },
		{ "ScaleRUV", Type::Float },
	});
}

void UDXEngineClient::LoadProperties(const NameString& from)
{
	NameString name_from = from;

	if (from == "")
		name_from = NameString(Class);

	// GetIniFile hands over a copy, held for the whole read
	std::unique_ptr<IniFile> iniFile = engine->packages->GetIniFile("System");
	const IniFile& ini = *iniFile;
	StartupFullscreen = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "StartupFullscreen", StartupFullscreen);
	WindowedViewportX = IniPropertyConverter<int>::FromIniFile(ini, name_from, "WindowedViewportX", WindowedViewportX);
	WindowedViewportY = IniPropertyConverter<int>::FromIniFile(ini, name_from, "WindowedViewportY", WindowedViewportY);
	WindowedColorBits = IniPropertyConverter<int>::FromIniFile(ini, name_from, "WindowedColorBits", WindowedColorBits);
	FullscreenViewportX = IniPropertyConverter<int>::FromIniFile(ini, name_from, "FullscreenViewportX", FullscreenViewportX);
	FullscreenViewportY = IniPropertyConverter<int>::FromIniFile(ini, name_from, "FullscreenViewportY", FullscreenViewportY);
	FullscreenColorBits = IniPropertyConverter<int>::FromIniFile(ini, name_from, "FullscreenColorBits", FullscreenColorBits);
	Brightness = IniPropertyConverter<float>::FromIniFile(ini, name_from, "Brightness", Brightness);
	BrightnessInUse = Brightness;
	UseJoystick = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "UseJoystick", UseJoystick);
	UseDirectInput = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "UseDirectInput", UseDirectInput);
	MinDesiredFrameRate = IniPropertyConverter<float>::FromIniFile(ini, name_from, "MinDesiredFrameRate", MinDesiredFrameRate);
	ScreenFlashes = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "ScreenFlashes", ScreenFlashes);
	Decals = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "Decals", Decals);
	NoDynamicLights = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "NoDynamicLights", NoDynamicLights);
	NoLighting = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "NoLighting", NoLighting);
	// A detail by its name's token, as the original's import reads the enum
	// byte: an ini's "Medium " is Medium, the blank after it the value's own
	// as the file is read; a name that is none keeps the default
	std::string detail;
	if (ini.FindValue(name_from, "TextureDetail", detail))
		ImportEnumText(detail, DetailNames, 4, TextureDetail);
	if (ini.FindValue(name_from, "SkinDetail", detail))
		ImportEnumText(detail, DetailNames, 4, SkinDetail);
	UpdateDetailLevels();
	DefineExtra();
	Extra.Load(ini, name_from);
}

void UDXEngineClient::UpdateDetailLevels()
{
	TextureLODSet[1] = std::min<int>(TextureDetail, 3);
	TextureLODSet[2] = std::min<int>(SkinDetail, 3);
}

void UDXEngineClient::SaveConfig()
{
	auto set = [&](const char* key, const std::string& value) { engine->packages->SetIniValue("System", Class, key, value); };
	set("StartupFullscreen", IniPropertyConverter<bool>::ToString(StartupFullscreen));
	set("WindowedViewportX", IniPropertyConverter<int>::ToString(WindowedViewportX));
	set("WindowedViewportY", IniPropertyConverter<int>::ToString(WindowedViewportY));
	set("WindowedColorBits", IniPropertyConverter<int>::ToString(WindowedColorBits));
	set("FullscreenViewportX", IniPropertyConverter<int>::ToString(FullscreenViewportX));
	set("FullscreenViewportY", IniPropertyConverter<int>::ToString(FullscreenViewportY));
	set("FullscreenColorBits", IniPropertyConverter<int>::ToString(FullscreenColorBits));
	set("Brightness", IniPropertyConverter<float>::ToString(Brightness));
	set("UseJoystick", IniPropertyConverter<bool>::ToString(UseJoystick));
	set("UseDirectInput", IniPropertyConverter<bool>::ToString(UseDirectInput));
	set("MinDesiredFrameRate", IniPropertyConverter<float>::ToString(MinDesiredFrameRate));
	set("ScreenFlashes", IniPropertyConverter<bool>::ToString(ScreenFlashes));
	set("Decals", IniPropertyConverter<bool>::ToString(Decals));
	set("NoDynamicLights", IniPropertyConverter<bool>::ToString(NoDynamicLights));
	set("NoLighting", IniPropertyConverter<bool>::ToString(NoLighting));
	set("TextureDetail", DetailNames[std::min<int>(TextureDetail, 3)]);
	set("SkinDetail", DetailNames[std::min<int>(SkinDetail, 3)]);
	Extra.ForEach([&](const std::string& name, const std::string& value) { engine->packages->SetIniValue("System", Class, name, value); });
}

bool UDXEngineClient::HasProperty(const NameString& propertyName) const
{
	static const char* names[] = {
		"Class", "StartupFullscreen", "WindowedViewportX", "WindowedViewportY", "WindowedColorBits",
		"FullscreenViewportX", "FullscreenViewportY", "FullscreenColorBits", "Brightness", "UseJoystick",
		"UseDirectInput", "MinDesiredFrameRate", "ScreenFlashes", "Decals", "NoDynamicLights", "NoLighting",
		"TextureDetail", "SkinDetail"
	};
	for (const char* name : names)
	{
		if (propertyName == NameString(name))
			return true;
	}
	return Extra.Has(propertyName);
}

std::string UDXEngineClient::GetPropertyAsString(const NameString& propertyName) const
{
	if (propertyName == "Class")
		return "class'" + Class + "'";
	else if (propertyName == "StartupFullscreen")
		return IniPropertyConverter<bool>::ToString(StartupFullscreen);
	else if (propertyName == "WindowedViewportX")
		return IniPropertyConverter<int>::ToString(WindowedViewportX);
	else if (propertyName == "WindowedViewportY")
		return IniPropertyConverter<int>::ToString(WindowedViewportY);
	else if (propertyName == "WindowedColorBits")
		return IniPropertyConverter<int>::ToString(WindowedColorBits);
	else if (propertyName == "FullscreenViewportX")
		return IniPropertyConverter<int>::ToString(FullscreenViewportX);
	else if (propertyName == "FullscreenViewportY")
		return IniPropertyConverter<int>::ToString(FullscreenViewportY);
	else if (propertyName == "FullscreenColorBits")
		return IniPropertyConverter<int>::ToString(FullscreenColorBits);
	else if (propertyName == "Brightness")
		return IniPropertyConverter<float>::ToString(Brightness);
	else if (propertyName == "UseJoystick")
		return IniPropertyConverter<bool>::ToString(UseJoystick);
	else if (propertyName == "UseDirectInput")
		return IniPropertyConverter<bool>::ToString(UseDirectInput);
	else if (propertyName == "MinDesiredFrameRate")
		return IniPropertyConverter<float>::ToString(MinDesiredFrameRate);
	else if (propertyName == "ScreenFlashes")
		return IniPropertyConverter<bool>::ToString(ScreenFlashes);
	else if (propertyName == "Decals")
		return IniPropertyConverter<bool>::ToString(Decals);
	else if (propertyName == "NoDynamicLights")
		return IniPropertyConverter<bool>::ToString(NoDynamicLights);
	else if (propertyName == "NoLighting")
		return IniPropertyConverter<bool>::ToString(NoLighting);
	else if (propertyName == "TextureDetail")
		return DetailNames[std::min<int>(TextureDetail, 3)];
	else if (propertyName == "SkinDetail")
		return DetailNames[std::min<int>(SkinDetail, 3)];
	else if (Extra.Has(propertyName))
		return Extra.Get(propertyName);

	// As the original's GET answers a name its class has not (UObject's
	// StaticExec, Core.dll 0x10153110)
	return "Unrecognized property " + propertyName.ToString();
}

// The original's UClient::PostEditChange (Engine.dll 0x10374ef0), as the
// client starts and after a SET of it: the live client's brightness held to
// 0 to 1 (and its MipFactor to -3 to 3, which TakeLiveValues holds), the
// class default not
void UDXEngineClient::PostEditChange()
{
	BrightnessInUse = clamp(BrightnessInUse, 0.0f, 1.0f);
}

void UDXEngineClient::TakeLiveValues()
{
	Brightness = BrightnessInUse;
	std::string mip = Extra.Get("MipFactor");
	float mipFactor = (float)strtod(mip.c_str(), nullptr);
	if (mipFactor < -3.0f || mipFactor > 3.0f)
		Extra.Set("MipFactor", IniPropertyConverter<float>::ToString(clamp(mipFactor, -3.0f, 3.0f)));
}

// A SET as the original's import takes its text: a text the property's type
// refuses (a bool's "Yes", an int's word, a detail's number) leaves it as it
// was, quietly; a float's never fails (bugs.md ENG.32, ENG.29)
void UDXEngineClient::SetPropertyFromString(const NameString& propertyName, const std::string& value)
{
	try
	{
		if (propertyName == "WindowedViewportX")
			WindowedViewportX = IniPropertyConverter<int>::FromString(value);
		else if (propertyName == "WindowedViewportY")
			WindowedViewportY = IniPropertyConverter<int>::FromString(value);
		else if (propertyName == "WindowedColorBits")
			WindowedColorBits = IniPropertyConverter<int>::FromString(value);
		else if (propertyName == "StartupFullscreen")
			StartupFullscreen = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "FullscreenViewportX")
			FullscreenViewportX = IniPropertyConverter<int>::FromString(value);
		else if (propertyName == "FullscreenViewportY")
			FullscreenViewportY = IniPropertyConverter<int>::FromString(value);
		else if (propertyName == "FullscreenColorBits")
			FullscreenColorBits = IniPropertyConverter<int>::FromString(value);
		else if (propertyName == "Brightness")
			BrightnessInUse = Brightness = IniPropertyConverter<float>::FromString(value);
		else if (propertyName == "UseJoystick")
			UseJoystick = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "UseDirectInput")
			UseDirectInput = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "MinDesiredFrameRate")
			MinDesiredFrameRate = IniPropertyConverter<float>::FromString(value);
		else if (propertyName == "ScreenFlashes")
			ScreenFlashes = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "Decals")
			Decals = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "NoDynamicLights")
			NoDynamicLights = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "NoLighting")
			NoLighting = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "TextureDetail")
		{
			ImportEnumText(value, DetailNames, 4, TextureDetail);
			UpdateDetailLevels();
		}
		else if (propertyName == "SkinDetail")
		{
			ImportEnumText(value, DetailNames, 4, SkinDetail);
			UpdateDetailLevels();
		}
		else if (Extra.Has(propertyName))
			Extra.Set(propertyName, value);
	}
	catch (const std::exception&)
	{
	}
}
