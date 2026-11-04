#pragma once

#include "UObjectProperty.h"

class UClassProperty : public UObjectProperty
{
public:
	using UObjectProperty::UObjectProperty;

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

	std::string PrintValue(const void* data) override;
	void SetValueFromString(void* data, const std::string& valueString) override;
	size_t ImportText(const std::string& text, size_t pos, void* data) override;
	void Mark(GCMarker& marker) override;

	UClass* MetaClass = nullptr;
};
