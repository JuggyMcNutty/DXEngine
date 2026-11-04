
#include "Precomp.h"
#include "UClassProperty.h"
#include "Packages/Core/UClass.h"

void UClassProperty::Load(ObjectStream* stream)
{
	UObjectProperty::Load(stream);
	MetaClass = stream->ReadObject<UClass>();
}

void UClassProperty::Save(PackageStreamWriter* stream)
{
	UObjectProperty::Save(stream);
	stream->WriteObject(MetaClass);
}

std::string UClassProperty::PrintValue(const void* data)
{
	UObject* obj = *(UObject**)data;
	if (obj)
		return "Class'" + obj->package->GetPackageName().ToString() + '.' + obj->Name.ToString() + '\'';
	else
		return "None";
}

void UClassProperty::SetValueFromString(void* data, const std::string& valueString)
{
	ImportText(valueString, 0, data);
}

// The original's UClassProperty::ImportText (Core.dll 0x10167110): the
// object's import, then what it set None, though not refused, unless it is a
// class deriving from MetaClass (Object's own Class, which DXEngine makes
// without one, any class)
size_t UClassProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	pos = UObjectProperty::ImportText(text, pos, data);
	if (pos == ImportFailed)
		return ImportFailed;
	UObject*& value = *(UObject**)data;
	UClass* cls = UObject::TryCast<UClass>(value);
	bool derived = cls && !MetaClass;
	for (UStruct* s = cls; s && !derived; s = s->BaseStruct)
		derived = s == MetaClass;
	if (!derived)
		value = nullptr;
	return pos;
}

void UClassProperty::Mark(GCMarker& marker)
{
	UObjectProperty::Mark(marker);
	marker.MarkCode(MetaClass);
}
