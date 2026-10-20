
#include "Precomp.h"
#include "UObjectProperty.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Utils/StrTools.h"

UObjectProperty::UObjectProperty(NameString name, UClass* base, ObjectFlags flags) : UPropertyT(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueObject;
}

void UObjectProperty::Load(ObjectStream* stream)
{
	UProperty::Load(stream);
	ObjectClass = stream->ReadObject<UClass>();
}

void UObjectProperty::Save(PackageStreamWriter* stream)
{
	UProperty::Save(stream);
	stream->WriteObject(ObjectClass);
}

void UObjectProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Object);
	*static_cast<UObject**>(data) = stream->ReadObject<UObject>();
}

void UObjectProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	*static_cast<UObject**>(data) = stream->ReadObject<UObject>();
}

void UObjectProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Object;
}

void UObjectProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	stream->WriteObject(*static_cast<UObject**>(data));
}

std::string UObjectProperty::PrintValue(const void* data)
{
	UObject* obj = *(UObject**)data;
	if (obj)
		return obj->Class->Name.ToString() + '\'' + obj->Name.ToString()/*obj->package->GetExportName(obj->exportIndex)*/ + '\'';
	else
		return "None";
}

bool UObjectProperty::IsDefaultValue(void* val)
{
	return *(UObject**)val == nullptr;
}

void UObjectProperty::SetValueFromString(void* data, const std::string& valueString)
{
	if (valueString.empty())
		return;

	UObject** propertyValue = (UObject**)data;

	// None clears it; Pkg.Name, or Type'Pkg.Name', names an object of the
	// property's class in that package, in any group
	std::string path = valueString;
	size_t quote = path.find('\'');
	if (quote != std::string::npos && path.size() > quote + 1 && path.back() == '\'' && path.substr(0, quote) != "Class" && path.substr(0, quote) != "class")
		path = path.substr(quote + 1, path.size() - quote - 2);
	size_t dot = path.find('.');

	if (StrTools::equals_ignore_case(valueString, "None"))
	{
		*propertyValue = nullptr;
	}
	else if (valueString.substr(0, 6) == "Class\'" || valueString.substr(0, 6) == "class\'")
	{
		*propertyValue = package->GetPackageManager()->FindClass(valueString.substr(6, valueString.length() - 7));
	}
	else if (dot != std::string::npos && path.find('(') == std::string::npos && ObjectClass)
	{
		try
		{
			Package* pkg = package->GetPackageManager()->GetPackage(path.substr(0, dot));
			if (UObject* obj = pkg->LoadObjectAnyGroup(ObjectClass, path.substr(path.rfind('.') + 1)))
				*propertyValue = obj;
		}
		catch (const std::exception&)
		{
		}
	}
	else
	{
		// This code is trying to set properties recursively into objects that already exists.
		// Is this something UE1 actually does?

		UObject* obj = *propertyValue;
		if (obj)
		{
			auto parsedProperties = ParsePropertiesFromString(valueString);

			for (auto& prop : parsedProperties)
				obj->SetPropertyFromString(prop.first, prop.second);
		}
	}
}

bool UObjectProperty::ComputeContainsRefs()
{
	return true;
}

void UObjectProperty::MarkValue(GCMarker& marker, void* data)
{
	marker.Mark(*static_cast<UObject**>(data));
}

void UObjectProperty::Mark(GCMarker& marker)
{
	UProperty::Mark(marker);
	marker.MarkCode(ObjectClass);
}
