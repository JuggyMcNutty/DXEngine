
#include "Precomp.h"
#include "UFixedArrayProperty.h"

UFixedArrayProperty::UFixedArrayProperty(NameString name, UClass* base, ObjectFlags flags) : UProperty(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueFixedArray;
}

// The original's Serialize (Core.dll 0x10167df0): UProperty's, the inner
// property, the count in four bytes. Its Link (0x10167c60) loads the inner
// first and takes the array's size from it, so the inner is loaded here
// before the struct holding the array lays it out. No inner, or a count that
// is no array's (negative, or past 16 M elements), is refused.
void UFixedArrayProperty::Load(ObjectStream* stream)
{
	UProperty::Load(stream);
	Inner = stream->ReadObject<UProperty>();
	Count = stream->ReadInt32();
	if (!Inner)
		Exception::Throw("Fixed array " + Name.ToString() + " has no inner property");
	if (Count < 0 || Count > 16 * 1024 * 1024)
		Exception::Throw("Fixed array " + Name.ToString() + " of " + std::to_string(Count) + " elements");
	Inner->LoadNow();
}

void UFixedArrayProperty::Save(PackageStreamWriter* stream)
{
	UProperty::Save(stream);
	stream->WriteObject(Inner);
	stream->WriteInt32(Count);
}

// Each element as its inner reads one, as the original's SerializeItem has a
// fixed array (Core.dll 0x10167da0): a tag's, a struct member's and a
// dynamic array's element's alike
void UFixedArrayProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_FixedArray);
	LoadStructMemberValue(data, stream);
}

void UFixedArrayProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
		Inner->LoadStructMemberValue(static_cast<uint8_t*>(data) + i * pitch, stream);
}

void UFixedArrayProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_FixedArray;
}

// Its Count elements, each as its inner writes one (SerializeItem)
void UFixedArrayProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
		Inner->SaveStructMemberValue(static_cast<uint8_t*>(data) + i * pitch, stream);
}

size_t UFixedArrayProperty::ElementAlignment()
{
	return Inner->ElementAlignment();
}

// Count of its inner's elements, as the original's Link sizes it
size_t UFixedArrayProperty::ElementSize()
{
	return Inner->ElementPitch() * Count;
}

void UFixedArrayProperty::ConstructElement(void* data)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
	{
		Inner->ConstructElement(static_cast<uint8_t*>(data) + i * pitch);
	}
}

void UFixedArrayProperty::CopyConstructElement(void* data, const void* src)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
	{
		Inner->CopyConstructElement(static_cast<uint8_t*>(data) + i * pitch, static_cast<const uint8_t*>(src) + i * pitch);
	}
}

void UFixedArrayProperty::DestructElement(void* data)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
	{
		Inner->DestructElement(static_cast<uint8_t*>(data) + i * pitch);
	}
}

void UFixedArrayProperty::CopyElement(void* data, const void* src)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
	{
		Inner->CopyElement(static_cast<uint8_t*>(data) + i * pitch, static_cast<const uint8_t*>(src) + i * pitch);
	}
}

bool UFixedArrayProperty::CompareElement(const void* v1, const void* v2)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
	{
		if (!Inner->CompareElement(static_cast<const uint8_t*>(v1) + i * pitch, static_cast<const uint8_t*>(v2) + i * pitch))
			return false;
	}
	return true;
}

bool UFixedArrayProperty::CompareLessElement(const void* v1, const void* v2)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
	{
		if (Inner->CompareLessElement(static_cast<const uint8_t*>(v1) + i * pitch, static_cast<const uint8_t*>(v2) + i * pitch))
			return true;
	}
	return false;
}

// The original's ExportTextItem (Core.dll 0x10167f50): "(a,b,c)", each
// element as its inner writes one delimited, a string quoted
std::string UFixedArrayProperty::PrintValue(const void* data)
{
	std::string print = "(";
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
	{
		if (i > 0)
			print += ",";
		print += Inner->PrintValue(static_cast<const uint8_t*>(data) + i * pitch);
	}
	return print + ")";
}

void UFixedArrayProperty::SetValueFromString(void* data, const std::string& valueString)
{
	ImportText(valueString, 0, data);
}

// The original's ImportText (Core.dll 0x10168050): from a "(", the whole
// array zeroed, then each element by its inner's delimited import, the one
// character after it passed over -- a "," but after the last, where any will
// do --, then a ")". So "(1,2,3)" sets 1, 2 and 3 and is refused, its ")"
// passed over as the last element's follower; "(1,2,3,)" is taken; "(1,2)"
// sets 1 and 2, zeroes the third and is refused; text with no "(" sets
// nothing. A refusal keeps what was set, as LoadConfig, SET,
// SetPropertyText and the travel pass it over. The text's end refuses it,
// where the original reads on past its terminator.
size_t UFixedArrayProperty::ImportText(const std::string& text, size_t pos, void* data)
{
	if (pos >= text.size() || text[pos] != '(')
		return ImportFailed;
	pos++;
	uint8_t* elements = static_cast<uint8_t*>(data);
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
	{
		Inner->DestructElement(elements + i * pitch);
		Inner->ConstructElement(elements + i * pitch);
	}
	for (int i = 0; i < Count; i++)
	{
		size_t next = Inner->ImportDelimitedText(text, pos, elements + i * pitch);
		if (next >= text.size())
			return ImportFailed;
		if (text[next] != ',' && i != Count - 1)
			return ImportFailed;
		pos = next + 1;
	}
	if (pos >= text.size() || text[pos] != ')')
		return ImportFailed;
	return pos + 1;
}

bool UFixedArrayProperty::ComputeContainsRefs()
{
	return Inner && Inner->ContainsRefs();
}

void UFixedArrayProperty::MarkValue(GCMarker& marker, void* data)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
		Inner->MarkValue(marker, static_cast<uint8_t*>(data) + i * pitch);
}

void UFixedArrayProperty::MarkNames(void* data)
{
	size_t pitch = Inner->ElementPitch();
	for (int i = 0; i < Count; i++)
		Inner->MarkNames(static_cast<uint8_t*>(data) + i * pitch);
}

void UFixedArrayProperty::Mark(GCMarker& marker)
{
	UProperty::Mark(marker);
	marker.MarkCode(Inner);
}
