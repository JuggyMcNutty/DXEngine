
#include "Precomp.h"
#include "PackageStream.h"
#include "Package.h"
#include "PackageWriter.h"
#include "Packages/Core/UObject.h"
#include "Utils/File.h"
#include <cstring>

PackageStream::PackageStream(Package* package, std::shared_ptr<File> file) : package(package), file(file)
{
}

void PackageStream::ReadBytes(void* d, uint32_t s)
{
	file->read(d, s);
}

int8_t PackageStream::ReadInt8()
{
	int8_t t;
	ReadBytes(&t, 1);
	return t;
}

int16_t PackageStream::ReadInt16()
{
	int16_t t;
	ReadBytes(&t, 2);
	return t;
}

int32_t PackageStream::ReadInt32()
{
	int32_t t;
	ReadBytes(&t, 4);
	return t;
}

int64_t PackageStream::ReadInt64()
{
	int64_t t;
	ReadBytes(&t, 8);
	return t;
}

float PackageStream::ReadFloat()
{
	float t;
	ReadBytes(&t, 4);
	return t;
}

uint8_t PackageStream::ReadUInt8()
{
	return ReadInt8();
}

uint16_t PackageStream::ReadUInt16()
{
	return ReadInt16();
}

uint32_t PackageStream::ReadUInt32()
{
	return ReadInt32();
}

uint64_t PackageStream::ReadUInt64()
{
	return ReadInt64();
}

void PackageStream::Seek(uint32_t offset)
{
	file->seek(offset);
}

void PackageStream::Skip(uint32_t bytes)
{
	file->seek(file->tell() + bytes);
}

uint32_t PackageStream::Tell()
{
	return (uint32_t)file->tell();
}

int32_t PackageStream::ReadIndex()
{
	uint8_t value = ReadInt8();
	bool signbit = value & (1 << 7);
	bool nextbyte = value & (1 << 6);
	int32_t index = value & 0x3f;
	if (nextbyte)
	{
		int shift = 6;
		do
		{
			value = ReadInt8();
			index |= static_cast<int32_t>(value & 0x7f) << shift;
			shift += 7;
		} while ((value & (1 << 7)) && shift < 32);
	}
	if (signbit)
		index = (int32_t)(0u - (uint32_t)index); // defined at INT_MIN, where -index is not
	return index;
}

// A name of the file's name table as the engine's text, UTF-8, as a name
// made from a file name, a URL, the ini or StringToName is: its 8-bit
// characters Latin-1, a byte a character, and one saved 16-bit, its count
// negative -- a name with a character past Latin-1, as the original's
// FString saves one -- its characters, as the original reads it (Core.dll
// 0x101459b0) and keeps them in its name table, each to its first NUL. A
// name past ASCII was its bytes as they were, which string() read as UTF-8,
// and a 16-bit one refused the package (bugs.md VM.25).
std::string PackageStream::ReadString()
{
	ScriptString s;
	if (GetVersion() >= 64)
	{
		int len = ReadIndex();
		// A name's length from the file: one up to 2 GB was zero-filled
		// before the read failed
		if (len < -1024 * 1024 || len > 1024 * 1024)
			Exception::Throw("PackageStream::ReadString: a string of " + std::to_string(len) + " characters");
		if (len < 0)
		{
			s.resize((size_t)-len);
			for (char16_t& c : s)
				c = (char16_t)ReadUInt16();
		}
		else
		{
			Array<char> bytes;
			bytes.resize(len);
			ReadBytes(bytes.data(), (int)bytes.size());
			s = Latin1ToScript(std::string_view(bytes.data(), bytes.size()));
		}
	}
	else
	{
		std::string bytes;
		while (true)
		{
			char c = ReadInt8();
			if (c == 0) break;
			bytes.push_back(c);
		}
		s = Latin1ToScript(bytes);
	}
	size_t end = s.find(u'\0');
	if (end != ScriptString::npos)
		s.resize(end);
	return ScriptToUTF8(s);
}

Package* PackageStream::GetPackage() const
{
	return package;
}

int PackageStream::GetVersion() const
{
	return package->GetVersion();
}

/////////////////////////////////////////////////////////////////////////////

PackageStreamWriter::PackageStreamWriter(PackageWriter* package, std::shared_ptr<File> file) : package(package), file(file)
{
}

void PackageStreamWriter::Close()
{
	file->close();
}

void PackageStreamWriter::WriteBytes(const void* d, uint32_t s)
{
	if (propertyPos == -1)
	{
		file->write(d, s);
	}
	else
	{
		size_t neededSize = (size_t)propertyPos + s;
		if (neededSize > propertyBuffer.size())
			propertyBuffer.resize(neededSize * 2);
		std::memcpy(propertyBuffer.data() + propertyPos, d, s);
		propertyPos += s;
	}
}

void PackageStreamWriter::WriteInt8(int8_t v)
{
	WriteBytes(&v, 1);
}

void PackageStreamWriter::WriteInt16(int16_t v)
{
	WriteBytes(&v, 2);
}

void PackageStreamWriter::WriteInt32(int32_t v)
{
	WriteBytes(&v, 4);
}

void PackageStreamWriter::WriteInt64(int64_t v)
{
	WriteBytes(&v, 8);
}

void PackageStreamWriter::WriteFloat(float v)
{
	WriteBytes(&v, 4);
}

void PackageStreamWriter::WriteUInt8(uint8_t v)
{
	WriteBytes(&v, 1);
}

void PackageStreamWriter::WriteUInt16(uint16_t v)
{
	WriteBytes(&v, 2);
}

void PackageStreamWriter::WriteUInt32(uint32_t v)
{
	WriteBytes(&v, 4);
}

void PackageStreamWriter::WriteUInt64(uint64_t v)
{
	WriteBytes(&v, 8);
}

void PackageStreamWriter::WriteIndex(int32_t v)
{
	uint8_t byte = 0;
	if (v < 0)
	{
		byte |= (1 << 7);
		v = -v;
	}
	byte |= (uint8_t)(v & 0x3f);
	v >>= 6;
	if (v != 0)
		byte |= (1 << 6);
	WriteUInt8(byte);

	while (v != 0)
	{
		byte = v & 0x7f;
		v >>= 7;
		if (v != 0)
			byte |= (1 << 7);
		WriteUInt8(byte);
	}
}

// The engine's own text, UTF-8, as a script's string is saved
void PackageStreamWriter::WriteString(const std::string& v)
{
	WriteScriptString(UTF8ToScript(v));
}

void PackageStreamWriter::WriteScriptString(const ScriptString& v)
{
	size_t len = v.find(u'\0');
	if (len == ScriptString::npos)
		len = v.size();
	bool wide = false;
	for (size_t i = 0; i < len && !wide; i++)
		wide = v[i] > 0xFF;
	if (wide)
	{
		WriteIndex(-(int)(len + 1));
		for (size_t i = 0; i < len; i++)
			WriteUInt16(v[i]);
		WriteUInt16(0);
	}
	else
	{
		WriteIndex((int)len + 1);
		for (size_t i = 0; i < len; i++)
			WriteUInt8((uint8_t)v[i]);
		WriteUInt8(0);
	}
}

void PackageStreamWriter::WriteAsciiZ(const std::string& v)
{
	WriteBytes(v.c_str(), (uint32_t)(v.size() + 1));
}

void PackageStreamWriter::WriteUnicodeZ(const std::wstring& v)
{
	WriteBytes(v.c_str(), (uint32_t)((v.size() + 1) * 2));
}

void PackageStreamWriter::WriteName(NameString name)
{
	WriteIndex(package->GetNameIndex(name));
}

void PackageStreamWriter::WriteObject(UObject* obj)
{
	WriteIndex(package->GetObjectReference(obj));
}

void PackageStreamWriter::BeginSkipOffset()
{
	skipOffsetLocation = Tell();
	WriteUInt32(0xffffffff);
}

void PackageStreamWriter::EndSkipOffset()
{
	uint32_t cur = Tell();
	Seek(skipOffsetLocation);
	WriteUInt32(cur);
	Seek(cur);
}

void PackageStreamWriter::Seek(uint32_t offset)
{
	file->seek(offset);
}

uint32_t PackageStreamWriter::Tell()
{
	return (uint32_t)file->tell();
}

PackageWriter* PackageStreamWriter::GetPackage() const
{
	return package;
}

int PackageStreamWriter::GetVersion() const
{
	return package->GetVersion();
}

void PackageStreamWriter::BeginProperty(NameString name)
{
	if (propertyPos != -1)
		Exception::Throw("BeginProperty called inside BeginProperty");

	WriteName(name);
	propertyPos = 0;
}

void PackageStreamWriter::EndProperty(PropertyHeader header)
{
	if (propertyPos == -1)
		Exception::Throw("EndProperty called without BeginProperty");

	header.size = propertyPos;
	propertyPos = -1;

	uint8_t info = (uint8_t)header.type;

	if (header.type == UPT_Bool)
	{
		if (header.boolValue)
			info |= 0x80;
		info |= 5 << 4;
	}
	else
	{
		if (header.arrayIndex > 0)
			info |= 0x80;

		if (header.size == 1)
			info |= 0 << 4;
		else if (header.size == 2)
			info |= 1 << 4;
		else if (header.size == 4)
			info |= 2 << 4;
		else if (header.size == 12)
			info |= 3 << 4;
		else if (header.size == 16)
			info |= 4 << 4;
		else if (header.size <= 0xff)
			info |= 5 << 4;
		else if (header.size <= 0xffff)
			info |= 6 << 4;
		else
			info |= 7 << 4;
	}

	WriteInt8(info);

	if (header.type == UPT_Struct)
		WriteName(header.structName);

	switch ((info & 0x70) >> 4)
	{
	default: break;
	case 5: WriteUInt8(header.size); break;
	case 6: WriteUInt16(header.size); break;
	case 7: WriteUInt32(header.size); break;
	}

	if (header.type != UPT_Bool && header.arrayIndex > 0)
	{
		if (header.arrayIndex < 128)
		{
			WriteUInt8((uint8_t)header.arrayIndex);
		}
		else if (header.arrayIndex < 16384)
		{
			// Two bytes up to 16,383, the first's top bits 10, four past it,
			// as the original's tag writes them (Core.dll 0x1012a840)
			uint8_t byte1 = 0x80 | (header.arrayIndex >> 8);
			uint8_t byte2 = header.arrayIndex & 0xff;
			WriteUInt8(byte1);
			WriteUInt8(byte2);
		}
		else
		{
			uint8_t byte1 = 0xc0 | (header.arrayIndex >> 24);
			uint8_t byte2 = (header.arrayIndex >> 16) & 0xff;
			uint8_t byte3 = (header.arrayIndex >> 8) & 0xff;
			uint8_t byte4 = header.arrayIndex & 0xff;
			WriteUInt8(byte1);
			WriteUInt8(byte2);
			WriteUInt8(byte3);
			WriteUInt8(byte4);
		}
	}

	WriteBytes(propertyBuffer.data(), header.size);
}
