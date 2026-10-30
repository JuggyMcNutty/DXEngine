
#include "Precomp.h"
#include "UProperty.h"
#include "Package/PackageManager.h"
#include "Utils/AlignedAlloc.h"
#include "Utils/Logger.h"
#include <sstream>

void UProperty::Load(ObjectStream* stream)
{
	UField::Load(stream);

	ArrayDimension = stream->ReadInt32();
	PropFlags = (PropertyFlags)stream->ReadUInt32();
	Category = stream->ReadName();
	if (AllFlags(PropFlags, PropertyFlags::Net))
		ReplicationOffset = stream->ReadUInt16();
	if (stream->GetVersion() <= 61)
		PropFlags = (PropertyFlags)((uint32_t)PropFlags & ~0x00080040);
}

void UProperty::Save(PackageStreamWriter* stream)
{
	UField::Save(stream);

	// To do: we clear some old PropFlags during load.
	// Maybe store a copy of the original PropFlags in Load so we can save them here?

	stream->WriteInt32(ArrayDimension);
	stream->WriteUInt32((uint32_t)PropFlags);
	stream->WriteName(Category);
	if (AllFlags(PropFlags, PropertyFlags::Net))
		stream->WriteUInt16(ReplicationOffset);
}

void UProperty::LoadValue(void* data, ObjectStream* stream, const PropertyHeader& header)
{
	ThrowIfTypeMismatch(header, UPT_Invalid);
}

void UProperty::LoadStructMemberValue(void* data, ObjectStream* stream)
{
	Exception::Throw("Unsupported struct member type");
}

void UProperty::SaveHeader(void* data, PropertyHeader& header)
{
	header.type = UPT_Invalid;
}

void UProperty::SaveValue(void* data, PackageStreamWriter* stream)
{
}

void UProperty::ThrowIfTypeMismatch(const PropertyHeader& header, UnrealPropertyType type)
{
	if (header.type != type)
		Exception::Throw("Property value does not match property type!");
}

void UProperty::MarkProperty(GCMarker& marker, void* data)
{
	if (!ContainsRefs())
		return;
	marker.SetField(this);
	for (int i = 0; i < ArrayDimension; i++)
		MarkValue(marker, GetElement(data, i));
}

void UProperty::MarkPropertyNames(void* data)
{
	if (!ContainsNames())
		return;
	for (int i = 0; i < ArrayDimension; i++)
		MarkNames(GetElement(data, i));
}

void UProperty::ConstructArray(void* data)
{
	for (int i = 0; i < ArrayDimension; i++)
	{
		ConstructElement(GetElement(data, i));
	}
}

void UProperty::CopyConstructArray(void* data, const void* src)
{
	for (int i = 0; i < ArrayDimension; i++)
	{
		CopyConstructElement(GetElement(data, i), GetElement(src, i));
	}
}

void UProperty::DestructArray(void* data)
{
	for (int i = 0; i < ArrayDimension; i++)
	{
		DestructElement(GetElement(data, i));
	}
}

void UProperty::CopyArray(void* data, const void* src)
{
	for (int i = 0; i < ArrayDimension; i++)
	{
		CopyElement(GetElement(data, i), GetElement(src, i));
	}
}

bool UProperty::CompareArray(const void* v1, const void* v2)
{
	for (int i = 0; i < ArrayDimension; i++)
	{
		if (!CompareElement(GetElement(v1, i), GetElement(v2, i)))
			return false;
	}
	return true;
}

bool UProperty::CompareLessArray(const void* v1, const void* v2)
{
	for (int i = 0; i < ArrayDimension; i++)
	{
		if (CompareLessElement(GetElement(v1, i), GetElement(v2, i)))
			return true;
	}
	return false;
}

void UProperty::GetExportText(std::string& buf, const std::string& whitespace, UObject* obj, UObject* defobj, int i)
{
	if (i >= ArrayDimension)
		Exception::Throw("UProperty::GetExportText index out of bounds");

	size_t elementSize = ElementSize();
	int offset = i * (int)elementSize;

	void* objval = static_cast<uint8_t*>(obj->PropertyData.Ptr(this)) + offset;
	void* defval = nullptr;
	try
	{
		defval = (defobj) ? static_cast<uint8_t*>(defobj->GetProperty(Name)) + offset : nullptr;
	}
	catch (...)
	{
		// ignore exceptions, its ok if GetProperty fails here
	}

	std::string innerbuf;
	GetExportText(innerbuf, whitespace, objval, defval, i);
	if (innerbuf.size() > 0)
		innerbuf += "\r\n";

	buf += innerbuf;
}

void UProperty::GetExportText(std::string& buf, const std::string& whitespace, void* objval, void* defval, int i)
{
	if (!defval && IsDefaultValue(objval))
		return;
	else if (defval && CompareElement(objval, defval))
		return;

	buf += whitespace + Name.ToString();
	if (ArrayDimension > 1)
		buf += '(' + std::to_string(i) + ')';
	buf += '=' + PrintValue(objval);
}

void UProperty::SetValueFromString(void* data, const std::string& valueString)
{
	//Exception::Throw("SetValueFromString() is unimplemented on this Property type!");
}

bool UProperty::ParseStructText(const std::string& text, Array<TextPair>& pairs)
{
	size_t p = 0, n = text.size();
	while (p < n && text[p] == ' ')
		p++;
	if (p >= n || text[p] != '(')
		return false;
	p++;
	while (true)
	{
		while (p < n && text[p] == ' ')
			p++;
		if (p >= n)
			return false;
		if (text[p] == ')')
			return true;
		TextPair pair;
		size_t keyStart = p;
		while (p < n && text[p] != '=' && text[p] != '[' && text[p] != ')' && text[p] != ',')
			p++;
		std::string key = text.substr(keyStart, p - keyStart);
		while (!key.empty() && key.back() == ' ')
			key.pop_back();
		if (p < n && text[p] == '[')
		{
			size_t q = p + 1;
			int index = 0;
			bool digits = false;
			while (q < n && text[q] >= '0' && text[q] <= '9')
			{
				index = index * 10 + (text[q] - '0');
				q++;
				digits = true;
			}
			if (q >= n || text[q] != ']' || !digits)
				return false;
			pair.Index = index;
			pair.Indexed = true;
			p = q + 1;
		}
		if (p >= n || text[p] != '=' || key.empty())
			return false;
		p++;
		size_t valueStart = p;
		int depth = 0;
		while (p < n)
		{
			char c = text[p];
			if (c == '"')
			{
				p++;
				while (p < n && text[p] != '"')
					p++;
				if (p >= n)
					return false;
				p++;
				continue;
			}
			if (c == '(')
				depth++;
			else if (c == ')')
			{
				if (depth == 0)
					break;
				depth--;
			}
			else if (c == ',' && depth == 0)
				break;
			p++;
		}
		if (p >= n)
			return false;
		pair.Key = NameString(key);
		pair.Value = text.substr(valueStart, p - valueStart);
		pairs.push_back(std::move(pair));
		if (text[p] == ',')
			p++;
	}
}

std::string UProperty::Unquote(const std::string& text)
{
	size_t p = text.find_first_not_of(' ');
	if (p == std::string::npos || text[p] != '"')
		return text;
	auto hex = [](char c) { return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1; };
	std::string out;
	for (p++; p < text.size() && text[p] != '"'; p++)
	{
		if (text[p] == '\\' && p + 1 < text.size())
		{
			if (text[p + 1] == '\\')
			{
				out += '\\';
				p++;
				continue;
			}
			int h1 = hex(text[p + 1]);
			int h2 = p + 2 < text.size() ? hex(text[p + 2]) : -1;
			out += (char)((h1 > 0 ? h1 : 0) * 16 + (h2 > 0 ? h2 : 0));
			p += 2;
			continue;
		}
		out += text[p];
	}
	return out;
}
