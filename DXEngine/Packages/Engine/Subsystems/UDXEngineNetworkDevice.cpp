
#include "Precomp.h"
#include "UDXEngineNetworkDevice.h"
#include "Engine.h"
#include "Package/PackageManager.h"

std::string UDXEngineNetworkDevice::GetPropertyAsString(const NameString& propertyName) const
{
	if (propertyName == "Class")
		return "class'" + Class + "'";

	LogMessage("Queried unknown property for DXEngineNetworkDevice: " + propertyName.ToString());
	return {};
}

void UDXEngineNetworkDevice::SetPropertyFromString(const NameString& propertyName, const std::string& value)
{
	LogMessage("Setting unknown property for DXEngineNetworkDevice: " + propertyName.ToString());
	engine->packages->SetIniValue("System", Class, propertyName, value);
}
