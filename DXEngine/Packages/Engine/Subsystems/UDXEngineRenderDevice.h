#pragma once

#include "URenderDevice.h"
#include "Package/IniProperty.h"

class UDXEngineRenderDevice : public URenderDevice
{
public:
	using URenderDevice::URenderDevice;

	std::string Class = "Engine.DXEngineRenderDevice";
	bool VolumetricLighting = true;
	bool ShinySurfaces = true;
	bool Coronas = true;
	bool HighDetailActors = true;
	bool DetailTextures = true; // the world's detail textures drawn (the Display menu's Detail Textures)
	bool UsePrecache = false; // D3DDrv's: the frame after a map's load shows PRECACHING

	void LoadProperties(const NameString& from = "") override;
	void SaveConfig() override;

	// The software device in the original's WindowedRenderDevice's place
	// (-nohard, or a GPU device that did not start): that class's settings,
	// read from its own section, where its SET saves them; the engine's own
	// section is left to the device GameRenderDevice names (bugs.md ENG.31)
	void TakeWindowedRenderDevice();
	bool IsWindowedRenderDevice() const { return !WindowedClass.empty(); }
	const std::string& WindowedRenderDeviceClass() const { return WindowedClass; }

	// A config property of the original's device class, the ini's
	// GameRenderDevice, or WindowedRenderDevice in its place: URenderDevice's
	// (Engine.dll 0x103713c0) and its own, D3DDrv's (0x10001880) or
	// SoftDrv's; any other name GET and SET answer "Unrecognized property"
	bool HasProperty(const NameString& propertyName) const;
	std::string GetPropertyAsString(const NameString& propertyName) const override;
	void SetPropertyFromString(const NameString& propertyName, const std::string& value) override;

private:
	void ReadSection(const IniFile& ini, const NameString& section, const std::string& device);

	bool IsD3DDrv = true;
	// The original's config properties DXEngine keeps no member for
	NativeConfigProperties Extra;
	// The WindowedRenderDevice's class, whose section holds its settings;
	// empty while they are the engine's own
	std::string WindowedClass;
};
