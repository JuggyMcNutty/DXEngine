
#include "Precomp.h"
#include "UArrayProperty.h"

UArrayProperty::UArrayProperty(NameString name, UClass* base, ObjectFlags flags) : UProperty(std::move(name), base, flags)
{
	ValueType = ExpressionValueType::ValueArray;
}

void UArrayProperty::Load(ObjectStream* stream)
{
	UProperty::Load(stream);
	Inner = stream->ReadObject<UProperty>();
}

void UArrayProperty::Save(PackageStreamWriter* stream)
{
	UProperty::Save(stream);
	stream->WriteObject(Inner);
}

void UArrayProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Array);

	// The data is the element's already (PropertyDataBlock::Load adds the
	// index times the pitch): indexing it again read past an ArrayProperty
	// whose dimension a package gave as more than 1. A count that is no array
	// (negative, or past 16 M elements) is refused before anything is
	// allocated.
	int arraySize = stream->ReadIndex();
	if (arraySize < 0 || arraySize > 16 * 1024 * 1024)
		Exception::Throw("Array " + Name.ToString() + " of " + std::to_string(arraySize) + " elements");
	ScriptArray& vec = *static_cast<ScriptArray*>(data);
	vec.Resize(arraySize);
	for (int i = 0; i < arraySize; i++)
	{
		Inner->LoadStructMemberValue(vec.GetItem(i), stream);
	}
}

void UArrayProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	int arraySize = stream->ReadIndex();
	if (arraySize < 0 || arraySize > 16 * 1024 * 1024)
		Exception::Throw("Array " + Name.ToString() + " of " + std::to_string(arraySize) + " elements");
	ScriptArray& vec = *static_cast<ScriptArray*>(data);
	vec.Resize(arraySize);
	for (int i = 0; i < arraySize; i++)
	{
		Inner->LoadStructMemberValue(vec.GetItem(i), stream);
	}
}

void UArrayProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Array;
}

void UArrayProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
	ScriptArray& vec = *static_cast<ScriptArray*>(data);
	stream->WriteIndex((int)vec.GetSize());
	// Each element as LoadValue reads it back (a bool's byte among them,
	// which SaveValue writes nothing for)
	for (size_t i = 0, count = vec.GetSize(); i < count; i++)
	{
		Inner->SaveStructMemberValue(vec.GetItem(i), stream);
	}
}

size_t UArrayProperty::ElementAlignment()
{
	return alignof(ScriptArray);
}

size_t UArrayProperty::ElementSize()
{
	return sizeof(ScriptArray);
}

void UArrayProperty::ConstructElement(void* data)
{
	new ((char*)data) ScriptArray(Inner);
}

void UArrayProperty::CopyConstructElement(void* data, const void* src)
{
	ConstructElement(data);
	CopyElement(data, src);
}

void UArrayProperty::DestructElement(void* data)
{
	auto vec = static_cast<ScriptArray*>(data);
	vec->~ScriptArray();
}

void UArrayProperty::CopyElement(void* data, const void* src)
{
	auto& vec = *static_cast<ScriptArray*>(data);
	auto& srcvec = *static_cast<const ScriptArray*>(src);
	vec = srcvec;
}

bool UArrayProperty::CompareElement(const void* a, const void* b)
{
	auto& avec = *static_cast<const ScriptArray*>(a);
	auto& bvec = *static_cast<const ScriptArray*>(b);
	return avec == bvec;
}

bool UArrayProperty::CompareLessElement(const void* a, const void* b)
{
	auto& avec = *static_cast<const ScriptArray*>(a);
	auto& bvec = *static_cast<const ScriptArray*>(b);
	return avec.GetSize() < bvec.GetSize();
}

void UArrayProperty::GetExportText(std::string& buf, const std::string& whitespace, UObject* obj, UObject* defobj, int i)
{
	if (i >= ArrayDimension)
		Exception::Throw("UArrayProperty::GetExportText index out of bounds");

	size_t elementPitch = ElementPitch();
	int offset = i * (int)elementPitch;

	ScriptArray* objarray = static_cast<ScriptArray*>(obj->GetProperty(Name)) + offset;
	ScriptArray* defarray = (defobj) ? static_cast<ScriptArray*>(defobj->GetProperty(Name)) + offset : nullptr;

	for (int k = 0; k < (int)objarray->GetSize(); k++)
	{
		void* objval = objarray->GetItem(k);
		void* defval = (defarray && (size_t)k < defarray->GetSize()) ? defarray->GetItem(k) : nullptr;

		Inner->GetExportText(buf, whitespace, objval, defval, i);
	}
}

// The original's UArrayProperty::ExportTextItem (Core.dll 0x10168720): the
// elements' text joined by "," within parentheses
std::string UArrayProperty::PrintValue(const void* data)
{
	ScriptArray& vec = *const_cast<ScriptArray*>(static_cast<const ScriptArray*>(data));
	std::string print = "(";
	for (size_t i = 0; i < vec.GetSize(); i++)
	{
		if (i > 0)
			print += ",";
		print += Inner->PrintValue(vec.GetItem(i));
	}
	return print + ")";
}

// The original's UArrayProperty::ImportText (Core.dll 0x10168830): "(a,b,...)",
// the array made as long as the elements, each element's delimited text to
// the inner property; text that is no such list leaves the array as it was
void UArrayProperty::SetValueFromString(void* data, const std::string& valueString)
{
	size_t p = valueString.find_first_not_of(' '), n = valueString.size();
	if (p == std::string::npos || valueString[p] != '(')
		return;
	Array<std::string> elements;
	p++;
	while (p < n && valueString[p] == ' ')
		p++;
	if (p < n && valueString[p] != ')')
	{
		size_t start = p;
		int depth = 0;
		while (p < n)
		{
			char c = valueString[p];
			if (c == '"')
			{
				p++;
				while (p < n && valueString[p] != '"')
					p++;
				if (p >= n)
					return;
				p++;
				continue;
			}
			if (c == '(')
				depth++;
			else if (c == ')' && depth > 0)
				depth--;
			else if ((c == ',' || c == ')') && depth == 0)
			{
				elements.push_back(valueString.substr(start, p - start));
				if (c == ')')
					break;
				start = p + 1;
			}
			p++;
		}
		if (p >= n)
			return;
	}
	ScriptArray& vec = *static_cast<ScriptArray*>(data);
	vec.Resize(elements.size());
	for (size_t i = 0; i < elements.size(); i++)
		Inner->SetDelimitedValueFromString(vec.GetItem(i), elements[i]);
}

bool UArrayProperty::ComputeContainsRefs()
{
	return Inner && Inner->ContainsRefs();
}

void UArrayProperty::MarkValue(GCMarker& marker, void* data)
{
	ScriptArray* array = static_cast<ScriptArray*>(data);
	size_t count = array->GetSize();
	for (size_t i = 0; i < count; i++)
		Inner->MarkValue(marker, array->GetItem(i));
}

void UArrayProperty::MarkNames(void* data)
{
	ScriptArray* array = static_cast<ScriptArray*>(data);
	size_t count = array->GetSize();
	for (size_t i = 0; i < count; i++)
		Inner->MarkNames(array->GetItem(i));
}

void UArrayProperty::Mark(GCMarker& marker)
{
	UProperty::Mark(marker);
	marker.MarkCode(Inner);
}
