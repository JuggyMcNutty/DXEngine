
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

// The original's UObjectProperty::ExportTextItem (Core.dll 0x10166d40):
// Type'Package.Group.Name', or None
std::string UObjectProperty::PrintValue(const void* data)
{
	UObject* obj = *(UObject**)data;
	if (obj)
		return obj->Class->Name.ToString() + '\'' + obj->GetPathName() + '\'';
	else
		return "None";
}

bool UObjectProperty::IsDefaultValue(void* val)
{
	return *(UObject**)val == nullptr;
}

// The original's UObjectProperty::ImportText (Core.dll 0x10166e20): None;
// Type'Name' an object of the class Type, else of the property's class, the
// name a path (Package.Group.Name) or bare, found among the loaded objects
// of that class (StaticFindObject in any package); nothing found leaves
// None. DXEngine loads a Package.Name not loaded yet, as its SET has always
// done; it once took a bare name for a property list to write into the
// object the property held.
void UObjectProperty::SetValueFromString(void* data, const std::string& valueString)
{
	size_t first = valueString.find_first_not_of(' ');
	if (first == std::string::npos)
		return;
	std::string text = valueString.substr(first, valueString.find_last_not_of(' ') - first + 1);
	UObject** propertyValue = (UObject**)data;

	if (StrTools::equals_ignore_case(text, "None"))
	{
		*propertyValue = nullptr;
		return;
	}
	UClass* cls = ObjectClass;
	std::string path = text;
	size_t quote = text.find('\'');
	if (quote != std::string::npos && text.size() > quote + 1 && text.back() == '\'')
	{
		path = text.substr(quote + 1, text.size() - quote - 2);
		UClass* named = package->GetPackageManager()->FindClassAnyPackage(text.substr(0, quote));
		if (!named)
		{
			*propertyValue = nullptr;
			return;
		}
		cls = named;
	}
	*propertyValue = package->GetPackageManager()->FindObjectAnyPackage(cls, path);
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
