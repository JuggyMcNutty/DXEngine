
#include "Precomp.h"
#include "LauncherSettings.h"
#include "Utils/JsonValue.h"
#include "Utils/File.h"
#include "Utils/Logger.h"
#include <algorithm>
#include <climits>

LauncherSettings& LauncherSettings::Get()
{
	static LauncherSettings settings;
	return settings;
}

static std::string GetSettingsFilename()
{
	return (Directory::engineUserDir() / "Settings.json").string();
}

// A key of the wrong type, a string where true or false or a number goes
// ("UseVSync": "true"), keeps its default and is said, as a key left out
// keeps its default; DXLauncher writes every key with its own type
static bool KeyTaken(const JsonValue& section, const char* sectionName, const char* key, JsonType type, const char* what)
{
	const JsonValue& value = section[key];
	if (value.is_undefined())
		return false;
	if (value.type() == type)
		return true;
	LogMessage("Settings.json: " + std::string(sectionName) + "." + key + " is not " + what + ": its default stands");
	return false;
}

static void ReadBool(const JsonValue& section, const char* sectionName, const char* key, bool& target)
{
	if (KeyTaken(section, sectionName, key, JsonType::boolean, "true or false"))
		target = section[key].to_boolean();
}

static void ReadFloat(const JsonValue& section, const char* sectionName, const char* key, float& target)
{
	if (KeyTaken(section, sectionName, key, JsonType::number, "a number"))
		target = (float)section[key].to_number();
}

// Held to an int's range as the double it is before the cast, which past
// that range is undefined (1e10, or 1e999, read as the infinity)
static void ReadInt(const JsonValue& section, const char* sectionName, const char* key, int& target)
{
	if (KeyTaken(section, sectionName, key, JsonType::number, "a number"))
		target = (int)std::clamp(section[key].to_number(), (double)INT_MIN, (double)INT_MAX);
}

static std::string ReadChoice(const JsonValue& section, const char* sectionName, const char* key)
{
	if (KeyTaken(section, sectionName, key, JsonType::string, "a name"))
		return section[key].to_string();
	return {};
}

LauncherSettings::LauncherSettings()
{
	try
	{
		JsonValue settings = JsonValue::parse(File::read_all_text(GetSettingsFilename()));

		const JsonValue& rendev = settings["RenderDevice"];
		std::string rendevtype = ReadChoice(rendev, "RenderDevice", "Type");
		if (rendevtype == "Vulkan")
			RenderDevice.Type = RenderDeviceType::Vulkan;
		else if (rendevtype == "D3D11")
			RenderDevice.Type = RenderDeviceType::D3D11;
		else if (rendevtype == "OpenGL")
			RenderDevice.Type = RenderDeviceType::OpenGL;
		else if (rendevtype == "GLES")
			RenderDevice.Type = RenderDeviceType::GLES;
		else if (rendevtype == "Software")
			RenderDevice.Type = RenderDeviceType::Software;
		RenderDevice.TypeNamed = rendevtype == "Vulkan" || rendevtype == "D3D11" || rendevtype == "OpenGL" || rendevtype == "GLES" || rendevtype == "Software";
		ReadBool(rendev, "RenderDevice", "CPUDriver", RenderDevice.CPUDriver);
		RenderDevice.Device = ReadChoice(rendev, "RenderDevice", "Device");

		std::string rendevaa = ReadChoice(rendev, "RenderDevice", "Antialias");
		if (rendevaa == "Off")
			RenderDevice.Antialias = AntialiasMode::Off;
		else if (rendevaa == "MSAA2x")
			RenderDevice.Antialias = AntialiasMode::MSAA2x;
		else if (rendevaa == "MSAA4x")
			RenderDevice.Antialias = AntialiasMode::MSAA4x;

		std::string rendevlight = ReadChoice(rendev, "RenderDevice", "Light");
		if (rendevlight == "Normal")
			RenderDevice.Light = LightMode::Normal;
		else if (rendevlight == "OneX")
			RenderDevice.Light = LightMode::OneX;
		else if (rendevlight == "BrighterActors")
			RenderDevice.Light = LightMode::BrighterActors;

		std::string rendevgamma = ReadChoice(rendev, "RenderDevice", "Gamma");
		if (rendevgamma == "D3D9")
			RenderDevice.Gamma = GammaMode::D3D9;
		else if (rendevgamma == "XOpenGL")
			RenderDevice.Gamma = GammaMode::XOpenGL;

		// Absent keys keep the header's defaults (UseVSync on, the scales 128),
		// as the Gamepad block's do: a file without them read vsync off and
		// each scale's floor
		ReadBool(rendev, "RenderDevice", "GammaCorrectScreenshots", RenderDevice.GammaCorrectScreenshots);
		ReadBool(rendev, "RenderDevice", "UseVSync", RenderDevice.UseVSync);
		ReadBool(rendev, "RenderDevice", "Hdr", RenderDevice.Hdr);
		ReadInt(rendev, "RenderDevice", "HdrScale", RenderDevice.HdrScale);
		ReadBool(rendev, "RenderDevice", "Bloom", RenderDevice.Bloom);
		ReadInt(rendev, "RenderDevice", "BloomAmount", RenderDevice.BloomAmount);
		ReadBool(rendev, "RenderDevice", "UseDebugLayer", RenderDevice.UseDebugLayer);
		ReadFloat(rendev, "RenderDevice", "Contrast", RenderDevice.Contrast);
		ReadFloat(rendev, "RenderDevice", "Saturation", RenderDevice.Saturation);
		ReadFloat(rendev, "RenderDevice", "GammaOffset", RenderDevice.GammaOffset);
		ReadFloat(rendev, "RenderDevice", "GammaOffsetRed", RenderDevice.GammaOffsetRed);
		ReadFloat(rendev, "RenderDevice", "GammaOffsetGreen", RenderDevice.GammaOffsetGreen);
		ReadFloat(rendev, "RenderDevice", "GammaOffsetBlue", RenderDevice.GammaOffsetBlue);
		ReadFloat(rendev, "RenderDevice", "LODBias", RenderDevice.LODBias);

		// Absent members keep their defaults: a missing bool would otherwise
		// read as false and quietly turn controller support off.
		const JsonValue& pad = settings["Gamepad"];
		ReadBool(pad, "Gamepad", "Enabled", Gamepad.Enabled);
		ReadFloat(pad, "Gamepad", "DeadZone", Gamepad.DeadZone);
		ReadFloat(pad, "Gamepad", "LookSensitivityX", Gamepad.LookSensitivityX);
		ReadFloat(pad, "Gamepad", "LookSensitivityY", Gamepad.LookSensitivityY);
		ReadBool(pad, "Gamepad", "InvertY", Gamepad.InvertY);
		ReadFloat(pad, "Gamepad", "CursorSpeed", Gamepad.CursorSpeed);
		ReadBool(pad, "Gamepad", "DXController", Gamepad.DXController);

		const JsonValue& perf = settings["Performance"];
		ReadFloat(perf, "Performance", "RenderScale", Performance.RenderScale);
	}
	catch (const std::exception& e)
	{
		// Said, not dropped in silence: every renderer and pad setting then
		// stands at its default. The error's own line alone: an engine
		// error's text goes on with where it was thrown, which is no news of
		// a file that is not there, as in a run by hand.
		std::string why = e.what();
		why = why.substr(0, why.find('\n'));
		LogMessage("Settings.json was not read (" + why + "): the defaults stand");
	}
	catch (...)
	{
		LogMessage("Settings.json was not read: the defaults stand");
	}
}
