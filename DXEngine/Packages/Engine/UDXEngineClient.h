#pragma once

#include "UClient.h"
#include "Package/IniProperty.h"

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
	// the ini's as read, a SET's as set, then held to 0 to 1 as the client
	// starts and by any SET of it (PostEditChange); Brightness is the class
	// default, which GET answers and the ini keeps
	float BrightnessInUse = 0.5f;
	bool UseJoystick = false;
	bool UseDirectInput = true;
	float MinDesiredFrameRate = 200.0f;
	bool ScreenFlashes = true;
	bool Decals = true;
	bool NoDynamicLights = false;
	// No light maps and every mesh unlit, as the original's client's
	// NoLighting (RenderSubsystem::SceneLit); off by default
	bool NoLighting = false;
	// The original's enum bytes (UClient::StaticConstructor, Engine.dll
	// 0x10375010): High 0, Medium 1, Low 2 and "Ultra Low" 3, which no text
	// reaches, its name a token and a word (bugs.md ENG.29)
	uint8_t TextureDetail = 0;
	uint8_t SkinDetail = 0;
	static constexpr const char* DetailNames[4] = { "High", "Medium", "Low", "Ultra Low" };

	// The mips a texture of a LODSet skips: TextureDetail's for the world's,
	// SkinDetail's for skins', none for any other (High 0, Medium 1, Low 2,
	// Ultra Low 3), as the original's UClient::TextureLODSet.
	int DetailLevel(int lodSet) const { return lodSet >= 0 && lodSet < 3 ? TextureLODSet[lodSet] : 0; }

	void LoadProperties(const NameString& from = "");
	void SaveConfig() override;

	// A config property of the original's UClient or UWindowsClient: any
	// other name GET and SET answer "Unrecognized property"
	bool HasProperty(const NameString& propertyName) const;
	std::string GetPropertyAsString(const NameString& propertyName) const override;
	void SetPropertyFromString(const NameString& propertyName, const std::string& value) override;
	void PostEditChange() override;

	// The live client's values taken as its config's, as a resize of the
	// original's viewport saves the client object and its class default is
	// read again from what that wrote: the brightness as held, and MipFactor
	// held to -3 to 3 (bugs.md ENG.30)
	void TakeLiveValues();

private:
	void UpdateDetailLevels();
	int TextureLODSet[3] = {};
	// The original's config properties DXEngine keeps no member for
	NativeConfigProperties Extra;
	void DefineExtra();
};
