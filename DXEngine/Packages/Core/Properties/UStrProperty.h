#pragma once

#include "UProperty.h"

class UStrProperty : public UPropertyT<ScriptString>
{
public:
	UStrProperty(NameString name, UClass* base, ObjectFlags flags);

	void LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header) override;
	void LoadStructMemberValue(void* data, ObjectStream* stream) override;

	void SaveHeader(void* data, PropertyHeader& header) override;
	void SaveValue(void* data, PackageStreamWriter* stream) override;

	// Without case, as the original's Identical (UStrProperty.cpp)
	bool CompareElement(const void* v1, const void* v2) override;

	void SetValueFromString(void* data, const std::string& valueString) override;
	void SetDelimitedValueFromString(void* data, const std::string& valueString) override;
	size_t ImportText(const std::string& text, size_t pos, void* data) override;
	size_t ImportDelimitedText(const std::string& text, size_t pos, void* data) override;
	std::string PrintValue(const void* data) override;
	bool IsDefaultValue(void* val) override;
};
