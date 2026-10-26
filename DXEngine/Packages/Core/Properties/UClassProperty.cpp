
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

// The original's UClassProperty::ImportText (Core.dll 0x10167110): the
// object's import, then None unless it is a class deriving from MetaClass
void UClassProperty::SetValueFromString(void* data, const std::string& valueString)
{
	UObjectProperty::SetValueFromString(data, valueString);
	UObject*& value = *(UObject**)data;
	UClass* cls = UObject::TryCast<UClass>(value);
	if (!cls)
	{
		value = nullptr;
		return;
	}
	if (MetaClass)
	{
		bool derived = false;
		for (UStruct* s = cls; s; s = s->BaseStruct)
		{
			if (s == MetaClass)
			{
				derived = true;
				break;
			}
		}
		if (!derived)
			value = nullptr;
	}
}

void UClassProperty::Mark(GCMarker& marker)
{
	UObjectProperty::Mark(marker);
	marker.MarkCode(MetaClass);
}
