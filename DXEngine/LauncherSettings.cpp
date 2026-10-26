
#include "Precomp.h"
#include "LauncherSettings.h"
#include "Utils/JsonValue.h"
#include "Utils/File.h"
#include "Utils/Logger.h"

LauncherSettings& LauncherSettings::Get()
{
	static LauncherSettings settings;
	return settings;
}

static std::string GetSettingsFilename()
{
	return (Directory::engineUserDir() / "Settings.json").string();
}

LauncherSettings::LauncherSettings()
{
	try
	{
		JsonValue settings = JsonValue::parse(File::read_all_text(GetSettingsFilename()));

		std::string rendevtype = settings["RenderDevice"]["Type"].to_string();
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
		RenderDevice.CPUDriver = settings["RenderDevice"]["CPUDriver"].to_boolean();

		std::string rendevaa = settings["RenderDevice"]["Antialias"].to_string();
		if (rendevaa == "Off")
			RenderDevice.Antialias = AntialiasMode::Off;
		else if (rendevaa == "MSAA2x")
			RenderDevice.Antialias = AntialiasMode::MSAA2x;
		else if (rendevaa == "MSAA4x")
			RenderDevice.Antialias = AntialiasMode::MSAA4x;

		std::string rendevlight = settings["RenderDevice"]["Light"].to_string();
		if (rendevlight == "Normal")
			RenderDevice.Light = LightMode::Normal;
		else if (rendevlight == "OneX")
			RenderDevice.Light = LightMode::OneX;
		else if (rendevlight == "BrighterActors")
			RenderDevice.Light = LightMode::BrighterActors;

		std::string rendevgamma = settings["RenderDevice"]["Gamma"].to_string();
		if (rendevgamma == "D3D9")
			RenderDevice.Gamma = GammaMode::D3D9;
		else if (rendevgamma == "XOpenGL")
			RenderDevice.Gamma = GammaMode::XOpenGL;

		// Absent keys keep the header's defaults (UseVSync on, the scales 128),
		// as the Gamepad block's do: a file without them read vsync off and
		// each scale's floor
		const JsonValue& rendev = settings["RenderDevice"];
		if (!rendev["GammaCorrectScreenshots"].is_undefined()) RenderDevice.GammaCorrectScreenshots = rendev["GammaCorrectScreenshots"].to_boolean();
		if (!rendev["UseVSync"].is_undefined()) RenderDevice.UseVSync = rendev["UseVSync"].to_boolean();
		if (!rendev["Hdr"].is_undefined()) RenderDevice.Hdr = rendev["Hdr"].to_boolean();
		if (!rendev["HdrScale"].is_undefined()) RenderDevice.HdrScale = rendev["HdrScale"].to_int();
		if (!rendev["Bloom"].is_undefined()) RenderDevice.Bloom = rendev["Bloom"].to_boolean();
		if (!rendev["BloomAmount"].is_undefined()) RenderDevice.BloomAmount = rendev["BloomAmount"].to_int();
		if (!rendev["UseDebugLayer"].is_undefined()) RenderDevice.UseDebugLayer = rendev["UseDebugLayer"].to_boolean();

		// Absent members keep their defaults: a missing bool would otherwise
		// read as false and quietly turn controller support off.
		const JsonValue& pad = settings["Gamepad"];
		if (!pad["Enabled"].is_undefined()) Gamepad.Enabled = pad["Enabled"].to_boolean();
		if (!pad["DeadZone"].is_undefined()) Gamepad.DeadZone = (float)pad["DeadZone"].to_number();
		if (!pad["LookSensitivityX"].is_undefined()) Gamepad.LookSensitivityX = (float)pad["LookSensitivityX"].to_number();
		if (!pad["LookSensitivityY"].is_undefined()) Gamepad.LookSensitivityY = (float)pad["LookSensitivityY"].to_number();
		if (!pad["InvertY"].is_undefined()) Gamepad.InvertY = pad["InvertY"].to_boolean();
		if (!pad["CursorSpeed"].is_undefined()) Gamepad.CursorSpeed = (float)pad["CursorSpeed"].to_number();

		const JsonValue& perf = settings["Performance"];
		if (!perf["RenderScale"].is_undefined()) Performance.RenderScale = (float)perf["RenderScale"].to_number();
	}
	catch (const std::exception& e)
	{
		// Said, not dropped in silence: every renderer and pad setting then
		// stands at its default
		LogMessage("Settings.json was not read (" + std::string(e.what()) + "): the defaults stand");
	}
	catch (...)
	{
		LogMessage("Settings.json was not read: the defaults stand");
	}
}
