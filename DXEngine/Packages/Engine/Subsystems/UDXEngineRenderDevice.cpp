#include "Precomp.h"
#include "UDXEngineRenderDevice.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Package/IniProperty.h"
#include "Utils/StrTools.h"

bool UDXEngineRenderDevice::HasProperty(const NameString& propertyName) const
{
	static const char* names[] = { "Class", "VolumetricLighting", "ShinySurfaces", "Coronas", "HighDetailActors", "DetailTextures" };
	for (const char* name : names)
	{
		if (propertyName == NameString(name))
			return true;
	}
	if (IsD3DDrv && propertyName == NameString("UsePrecache"))
		return true;
	return Extra.Has(propertyName);
}

std::string UDXEngineRenderDevice::GetPropertyAsString(const NameString& propertyName) const
{
	if (propertyName == "Class")
		return "class'" + Class + "'";
	else if (propertyName == "VolumetricLighting")
		return IniPropertyConverter<bool>::ToString(VolumetricLighting);
	else if (propertyName == "ShinySurfaces")
		return IniPropertyConverter<bool>::ToString(ShinySurfaces);
	else if (propertyName == "Coronas")
		return IniPropertyConverter<bool>::ToString(Coronas);
	else if (propertyName == "HighDetailActors")
		return IniPropertyConverter<bool>::ToString(HighDetailActors);
	else if (propertyName == "DetailTextures")
		return IniPropertyConverter<bool>::ToString(DetailTextures);
	else if (IsD3DDrv && propertyName == "UsePrecache")
		return IniPropertyConverter<bool>::ToString(UsePrecache);
	else if (Extra.Has(propertyName))
		return Extra.Get(propertyName);

	// As the original's GET answers a name its class has not (UObject's
	// StaticExec, Core.dll 0x10153110): Translucency among them, which no
	// device of the game's has (bugs.md D.62)
	return "Unrecognized property " + propertyName.ToString();
}

// A SET as the original's import takes its text: a text the property's type
// refuses (a bool's "Yes") leaves it as it was, quietly (bugs.md ENG.32)
void UDXEngineRenderDevice::SetPropertyFromString(const NameString& propertyName, const std::string& value)
{
	try
	{
		if (propertyName == "VolumetricLighting")
			VolumetricLighting = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "ShinySurfaces")
			ShinySurfaces = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "Coronas")
			Coronas = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "HighDetailActors")
			HighDetailActors = IniPropertyConverter<bool>::FromString(value);
		else if (propertyName == "DetailTextures")
			DetailTextures = IniPropertyConverter<bool>::FromString(value);
		else if (IsD3DDrv && propertyName == "UsePrecache")
			UsePrecache = IniPropertyConverter<bool>::FromString(value);
		else if (Extra.Has(propertyName))
			Extra.Set(propertyName, value);
	}
	catch (const std::exception&)
	{
	}
}

void UDXEngineRenderDevice::LoadProperties(const NameString& from)
{
	NameString name_from = from;

	if (from == "")
		name_from = NameString(Class);

	// GetIniFile hands over a copy, held for the whole read
	std::unique_ptr<IniFile> iniFile = engine->packages->GetIniFile("System");
	const IniFile& ini = *iniFile;
	VolumetricLighting = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "VolumetricLighting", VolumetricLighting);
	ShinySurfaces = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "ShinySurfaces", ShinySurfaces);
	Coronas = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "Coronas", Coronas);
	HighDetailActors = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "HighDetailActors", HighDetailActors);
	DetailTextures = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "DetailTextures", DetailTextures);
	UsePrecache = IniPropertyConverter<bool>::FromIniFile(ini, name_from, "UsePrecache", UsePrecache);

	// The original's class is the ini's GameRenderDevice: URenderDevice's
	// Description and DescFlags, and its own -- D3DDrv's options
	// (UD3DRenderDevice::StaticConstructor, D3DDrv.dll 0x10001880), SoftDrv's
	// smoothing, FastTranslucency and DetailBias. OpenGLDrv reads its
	// settings from the config itself and has none of its own.
	using Type = NativeConfigProperties::Type;
	std::string device = engine->packages->GetIniValue("system", "Engine.Engine", "GameRenderDevice", "D3DDrv.D3DRenderDevice");
	IsD3DDrv = StrTools::equals_ignore_case(device, "D3DDrv.D3DRenderDevice");
	if (IsD3DDrv)
	{
		Extra.Define({
			{ "Description", Type::String }, { "DescFlags", Type::Int },
			{ "UseMipmapping", Type::Bool }, { "UseTrilinear", Type::Bool }, { "UseMultitexture", Type::Bool },
			{ "UsePalettes", Type::Bool }, { "UseGammaCorrection", Type::Bool }, { "Use3dfx", Type::Bool },
			{ "UseTripleBuffering", Type::Bool }, { "UseVSync", Type::Bool }, { "UseVideoMemoryVB", Type::Bool },
			{ "UseAGPTextures", Type::Bool }, { "UseVertexFog", Type::Bool }, { "dwVendorId", Type::Int },
			{ "dwDeviceId", Type::Int },
		});
	}
	else if (StrTools::equals_ignore_case(device, "SoftDrv.SoftwareRenderDevice"))
	{
		Extra.Define({
			{ "Description", Type::String }, { "DescFlags", Type::Int },
			{ "HighResTextureSmooth", Type::Bool }, { "LowResTextureSmooth", Type::Bool },
			{ "FastTranslucency", Type::Bool }, { "DetailBias", Type::Float },
		});
	}
	else
	{
		Extra.Define({ { "Description", Type::String }, { "DescFlags", Type::Int } });
	}
	Extra.Load(ini, name_from);
}

void UDXEngineRenderDevice::SaveConfig()
{
	auto set = [&](const std::string& key, const std::string& value) { engine->packages->SetIniValue("System", Class, key, value); };
	set("VolumetricLighting", IniPropertyConverter<bool>::ToString(VolumetricLighting));
	set("ShinySurfaces", IniPropertyConverter<bool>::ToString(ShinySurfaces));
	set("Coronas", IniPropertyConverter<bool>::ToString(Coronas));
	set("HighDetailActors", IniPropertyConverter<bool>::ToString(HighDetailActors));
	set("DetailTextures", IniPropertyConverter<bool>::ToString(DetailTextures));
	set("UsePrecache", IniPropertyConverter<bool>::ToString(UsePrecache));
	Extra.ForEach(set);
}
