#pragma once

#include "URenderDevice.h"

class UDXEngineRenderDevice : public URenderDevice
{
public:
	using URenderDevice::URenderDevice;

	std::string Class = "Engine.DXEngineRenderDevice";
	bool Translucency = true;
	bool VolumetricLighting = true;
	bool ShinySurfaces = true;
	bool Coronas = true;
	bool HighDetailActors = true;
	bool DetailTextures = true; // the world's detail textures drawn (the Display menu's Detail Textures)
	bool UsePrecache = false; // D3DDrv's: the frame after a map's load shows PRECACHING

	void LoadProperties(const NameString& from = "") override;
	void SaveConfig() override;

	std::string GetPropertyAsString(const NameString& propertyName) const override;
	void SetPropertyFromString(const NameString& propertyName, const std::string& value) override;
};
