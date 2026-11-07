#pragma once

#include "UClient.h"

class UDXEngineClient : public UClient
{
public:
	using UClient::UClient;

	std::string Class = "Engine.DXEngineClient";
	bool StartupFullscreen = false;
	int WindowedViewportX = 1920;
	int WindowedViewportY = 1080;
	int WindowedColorBits = 32;
	int FullscreenViewportX = 0;
	int FullscreenViewportY = 0;
	int FullscreenColorBits = 32;
	float Brightness = 0.5f;
	// The brightness the frame takes, the live client's of the original:
	// the ini's as read, a SET's as set, then held to 0 to 1 by any SET of
	// the client (PostEditChange); Brightness is the class default, which
	// GET answers and the ini keeps
	float BrightnessInUse = 0.5f;
	bool UseJoystick = false;
	bool UseDirectInput = true;
	int MinDesiredFrameRate = 200;
	bool ScreenFlashes = true;
	bool Decals = true;
	bool NoDynamicLights = false;
	// No light maps and every mesh unlit, as the original's client's
	// NoLighting (RenderSubsystem::SceneLit); off by default
	bool NoLighting = false;
	std::string TextureDetail = "High";
	std::string SkinDetail = "High";

	// The mips a texture of a LODSet skips: TextureDetail's for the world's,
	// SkinDetail's for skins', none for any other (High 0, Medium 1, Low 2,
	// UltraLow 3), as the original's UClient::TextureLODSet.
	int DetailLevel(int lodSet) const { return lodSet >= 0 && lodSet < 3 ? TextureLODSet[lodSet] : 0; }

	void LoadProperties(const NameString& from = "");
	void SaveConfig() override;

	std::string GetPropertyAsString(const NameString& propertyName) const override;
	void SetPropertyFromString(const NameString& propertyName, const std::string& value) override;
	void PostEditChange() override;

private:
	void UpdateDetailLevels();
	int TextureLODSet[3] = {};
};
