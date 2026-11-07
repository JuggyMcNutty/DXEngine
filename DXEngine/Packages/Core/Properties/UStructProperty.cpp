
#include "Precomp.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "UStructProperty.h"
#include "Packages/Core/UStruct.h"

UStructProperty::UStructProperty(NameString name, UClass* base, ObjectFlags flags) : UProperty(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueStruct;
}

void UStructProperty::Load(ObjectStream* stream)
{
	UProperty::Load(stream);
	Struct = stream->ReadObject<UStruct>();
	Struct->LoadNow();

	if (Struct->Name == "Vector")
		ValueType = ExpressionValueType::ValueVector;
	else if (Struct->Name == "Rotator")
		ValueType = ExpressionValueType::ValueRotator;
	else if (Struct->Name == "Color")
		ValueType = ExpressionValueType::ValueColor;
	else if (Struct->Name == "Coords")
		ValueType = ExpressionValueType::ValueCoords;
	else if (Struct->Name == "Quat")
		ValueType = ExpressionValueType::ValueQuat;
}

void UStructProperty::Save(PackageStreamWriter* stream)
{
	UProperty::Save(stream);
	stream->WriteObject(Struct);
}

void UStructProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Struct);

	if (Struct->Name != header.structName)
		Exception::Throw("Encountered struct '" + header.structName.ToString() + "' does not match expected struct property '" + Struct->Name.ToString() + "'");

	LoadStructMemberValue(data, stream);
}

void UStructProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	if (Struct->Properties.empty())
		throw std::runtime_error("Struct has no properties");

	// Every element of a static array member, the native and transient
	// members passed over, as the original's UStruct::SerializeBin has it
	// (Core.dll 0x10126610); DXEngine read each member once, whatever its
	// dimension, and the native and transient ones with the rest
	for (UProperty* fieldprop : Struct->Properties)
	{
		if (AnyFlags(fieldprop->PropFlags, PropertyFlags::Native | PropertyFlags::Transient))
			continue;
		for (int i = 0; i < fieldprop->ArrayDimension; i++)
		{
			void* fielddata = (uint8_t*)data + fieldprop->DataOffset.DataOffset + i * fieldprop->ElementPitch();
			fieldprop->LoadStructMemberValue(fielddata, stream);
		}
	}
}

void UStructProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Struct;
	header.structName = Struct->Name;
}

void UStructProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	SaveStructMemberValue(data, stream);
}

void UStructProperty::SaveStructMemberValue(void* data, PackageStreamWriter* stream)
{
	if (Struct->Properties.empty())
		throw std::runtime_error("Struct has no properties");

	for (UProperty* fieldprop : Struct->Properties)
	{
		if (AnyFlags(fieldprop->PropFlags, PropertyFlags::Native | PropertyFlags::Transient))
			continue;
		for (int i = 0; i < fieldprop->ArrayDimension; i++)
		{
			void* fielddata = (uint8_t*)data + fieldprop->DataOffset.DataOffset + i * fieldprop->ElementPitch();
			fieldprop->SaveStructMemberValue(fielddata, stream);
		}
	}
}

size_t UStructProperty::ElementAlignment()
{
	return Struct ? Struct->StructAlignment : 1;
}

size_t UStructProperty::ElementSize()
{
	return Struct ? Struct->StructSize : 0;
}

void UStructProperty::ConstructElement(void* data)
{
	if (Struct)
	{
		for (UProperty* prop : Struct->Properties)
		{
			prop->ConstructArray(static_cast<uint8_t*>(data) + prop->DataOffset.DataOffset);
		}
	}
}

void UStructProperty::CopyConstructElement(void* data, const void* src)
{
	if (Struct)
	{
		for (UProperty* prop : Struct->Properties)
		{
			prop->CopyConstructArray(
				static_cast<uint8_t*>(data) + prop->DataOffset.DataOffset,
				static_cast<const uint8_t*>(src) + prop->DataOffset.DataOffset);
		}
	}
}

void UStructProperty::CopyElement(void* data, const void* src)
{
	if (Struct)
	{
		for (UProperty* prop : Struct->Properties)
		{
			prop->CopyArray(
				static_cast<uint8_t*>(data) + prop->DataOffset.DataOffset,
				static_cast<const uint8_t*>(src) + prop->DataOffset.DataOffset);
		}
	}
}

bool UStructProperty::NeedsDestruct()
{
	return !Struct || !Struct->DestructProps().empty();
}

void UStructProperty::DestructElement(void* data)
{
	if (Struct)
	{
		for (UProperty* prop : Struct->Properties)
		{
			prop->DestructArray(static_cast<uint8_t*>(data) + prop->DataOffset.DataOffset);
		}
	}
}

bool UStructProperty::CompareElement(const void* v1, const void* v2)
{
	return Struct ? Struct->IsEqual(v1, v2) : true;
}

bool UStructProperty::CompareLessElement(const void* v1, const void* v2)
{
	if (Struct)
	{
		for (UProperty* prop : Struct->Properties)
		{
			if (prop->CompareLessArray(
				static_cast<const uint8_t*>(v1) + prop->DataOffset.DataOffset,
				static_cast<const uint8_t*>(v2) + prop->DataOffset.DataOffset))
			{
				return true;
			}
		}
	}
	return false;
}

// A struct not loaded yet is taken to hold references: the answer is
// kept, and its properties are walked once they are there.
bool UStructProperty::ComputeContainsRefs()
{
	return Struct && (Struct->DelayLoad || !Struct->RefProps().empty());
}

bool UStructProperty::ComputeContainsNames()
{
	return Struct && (Struct->DelayLoad || !Struct->NameProps().empty());
}

void UStructProperty::MarkNames(void* data)
{
	if (Struct->DelayLoad)
		return;
	for (UProperty* prop : Struct->NameProps())
		prop->MarkPropertyNames(static_cast<uint8_t*>(data) + prop->DataOffset.DataOffset);
}

void UStructProperty::MarkValue(GCMarker& marker, void* data)
{
	if (Struct->DelayLoad)
		return;
	for (UProperty* prop : Struct->RefProps())
		prop->MarkProperty(marker, static_cast<uint8_t*>(data) + prop->DataOffset.DataOffset);
	marker.SetField(this);
}

void UStructProperty::Mark(GCMarker& marker)
{
	UProperty::Mark(marker);
	marker.MarkCode(Struct);
}

void UStructProperty::GetExportText(std::string& buf, const std::string& whitespace, UObject* obj, UObject* defobj, int i)
{
	if (!Struct)
	{
		buf += whitespace + Name.ToString() + '=' + "null struct";
		return;
	}

	if (i >= ArrayDimension)
		Exception::Throw("UStructProperty::GetExportText index out of bounds");

	uint8_t* objval = static_cast<uint8_t*>(obj->PropertyData.Ptr(this)) + (i * ElementPitch());
	uint8_t* defval = nullptr;
	try
	{
		// This very property's default, not the first of its name
		if (defobj)
			defval = static_cast<uint8_t*>(defobj->PropertyData.Ptr(this)) + (i * ElementPitch());
	}
	catch (...)
	{
	}

	std::string structbuf = "(";
	std::string innerbuf;

	for (UField* field = Struct->Children; field != nullptr; field = field->Next)
	{
		UProperty* fieldprop = UObject::TryCast<UProperty>(field);

		if (fieldprop == nullptr)
			continue;

		size_t offset = fieldprop->DataOffset.DataOffset;

		for (int k = 0; k < fieldprop->ArrayDimension; k++)
		{
			uint8_t* objsubval = objval + offset;
			uint8_t* defsubval = (defval) ? defval + offset : nullptr;
			fieldprop->GetExportText(innerbuf, "", objsubval, defsubval, k);

			if (innerbuf.length() > 0)
			{
				structbuf += innerbuf + ',';
				innerbuf.clear();
			}
		}
	}

	if (structbuf.length() > 1)
	{
		structbuf.pop_back();
		buf += whitespace + Name.ToString() + "=" + structbuf + ")\r\n";
	}
}

// The struct's members in the original's order (its TFieldIterator): the
// struct's own first, then those of the struct it extends, and so on up (a
// Plane prints W before X, Y and Z)
template<typename F>
static void ForEachMember(UStruct* strct, F&& f)
{
	for (UStruct* s = strct; s; s = s->BaseStruct)
	{
		for (UField* field = s->Children; field; field = field->Next)
		{
			if (UProperty* member = UObject::TryCast<UProperty>(field))
				if (!f(member))
					return;
		}
	}
}

// The original's UStructProperty::ExportTextItem (Core.dll 0x10169710): the
// members of the struct and of the structs it extends, its own first, each
// element of a static array as Name[i]=, joined by "," within parentheses;
// nothing at all for a struct with no member to print
std::string UStructProperty::PrintValue(const void* data)
{
	if (!Struct)
		return "null struct";
	std::string print;
	const uint8_t* d = (const uint8_t*)data;
	ForEachMember(Struct, [&](UProperty* member) {
		if (AnyFlags(member->PropFlags, PropertyFlags::Native | PropertyFlags::Transient))
			return true;
		for (int i = 0; i < member->ArrayDimension; i++)
		{
			print += print.empty() ? "(" : ",";
			print += member->Name.ToString();
			if (member->ArrayDimension > 1)
				print += "[" + std::to_string(i) + "]";
			print += "=" + member->PrintValue(d + member->DataOffset.DataOffset + i * member->ElementPitch());
		}
		return true;
	});
	if (!print.empty())
		print += ")";
	return print;
}

// The original's UStructProperty::ImportText (Core.dll 0x101698b0): the
// pairs of "(key=value,...)", each member found by name among the struct's
// and its supers', its own import given the value's text; a member not named
// keeps its value. A key with an element's index stops the import there,
// logged, the members before it set: the original's index reader starts on
// the bracket and finds none, so it never reads back the Name[i]= its own
// export writes.
void UStructProperty::SetValueFromString(void* data, const std::string& valueString)
{
	if (valueString.empty() || !Struct)
		return;
	if (valueString == "null" || valueString == "null struct" || StrTools::equals_ignore_case(valueString, "None"))
		return;

	Array<TextPair> pairs;
	if (!ParseStructText(valueString, pairs))
		LogMessage("Bad struct text for " + Name.ToString() + ": " + valueString);
	for (const TextPair& pair : pairs)
	{
		if (pair.Indexed)
		{
			LogMessage("ImportText: Illegal array element");
			return;
		}
		ForEachMember(Struct, [&](UProperty* member) {
			if (member->Name != pair.Key)
				return true;
			member->SetDelimitedValueFromString((uint8_t*)data + member->DataOffset.DataOffset, pair.Value);
			return false;
		});
	}
}
