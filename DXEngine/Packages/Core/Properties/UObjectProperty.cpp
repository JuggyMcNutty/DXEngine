
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

void UObjectProperty::SetValueFromString(void* data, const std::string& valueString)
{
	ImportText(valueString, 0, data);
}

// The original's UObjectProperty::ImportText (Core.dll 0x10166e20): the
// text's first token, dots allowed (ReadToken); None sets None. Else, past
// the blanks after it, Type'Name' is an object of the class Type, other text
// the token an object of the property's class, a path (Package.Group.Name)
// or bare, among the loaded objects of that class (StaticFindObject in any
// package): what is found is set, and nothing found sets None and is
// refused; a Type'Name' left open, or a Type no class names, is refused with
// the value kept. The character after those blanks is passed whatever it is
// (its *Buffer++ == '\'', 0x10166eba), so an unquoted object's text ends one
// past it: of "Terrorist 1" nothing of the 1 is left for a next parameter
// (bugs.md VM.21). DXEngine loads a Package.Name not loaded yet, as its SET
// has always done.
size_t UObjectProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	std::string token;
	if (!ReadToken(text, pos, token, true))
		return ImportFailed;
	UObject*& value = *(UObject**)data;
	if (StrTools::equals_ignore_case(token, "None"))
	{
		value = nullptr;
		return pos;
	}
	while (pos < text.size() && text[pos] == ' ')
		pos++;
	bool quoted = pos < text.size() && text[pos] == '\'';
	pos = std::min(pos + 1, text.size());
	UClass* cls = ObjectClass;
	if (quoted)
	{
		std::string path;
		if (!ReadToken(text, pos, path, true) || pos >= text.size() || text[pos] != '\'')
			return ImportFailed;
		pos++;
		cls = token.empty() ? nullptr : package->GetPackageManager()->FindClassAnyPackage(token);
		if (!cls)
			return ImportFailed;
		token = path;
	}
	// No object has an empty name (StaticFindObject finds no such name)
	value = token.empty() ? nullptr : package->GetPackageManager()->FindObjectAnyPackage(cls, token);
	return value ? pos : ImportFailed;
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
