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

	// A config property of the original's device class, the ini's
	// GameRenderDevice: URenderDevice's (Engine.dll 0x103713c0) and its
	// own, D3DDrv's (0x10001880) or SoftDrv's; any other name GET and SET
	// answer "Unrecognized property"
	bool HasProperty(const NameString& propertyName) const;
	std::string GetPropertyAsString(const NameString& propertyName) const override;
	void SetPropertyFromString(const NameString& propertyName, const std::string& value) override;

private:
	bool IsD3DDrv = true;
	// The original's config properties DXEngine keeps no member for
	NativeConfigProperties Extra;
};
