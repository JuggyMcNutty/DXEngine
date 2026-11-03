#pragma once

#include "Package.h"
#include <string.h>
#include "Utils/Exception.h"

enum class ObjectFlags : uint32_t;
class UClass;
class UObject;

class ObjectStream
{
public:
	ObjectStream(Package* package, std::unique_ptr<uint64_t[]> buf, size_t startoffset, size_t size, const NameString& name, UClass* base) : package(package), buffer(std::move(buf)), data(reinterpret_cast<const uint8_t*>(buffer.get())), startoffset(startoffset), size(size), name(name), base(base) { }

	void ReadBytes(void* d, uint32_t s)
	{
		if (pos + s > size)
			Exception::Throw("Unexpected end of file");
		if (s != 0) // a struct with no bytecode reads 0 bytes into nothing: memcpy's null is undefined
			memcpy(d, data + pos, s);
		pos += s;
	}

	void ThrowIfNotEnd()
	{
		if (pos != size)
			Exception::Throw("Unexpected bytes at end of object stream");
	}

	bool IsEmptyStream() const { return size == 0; }
	uint32_t Remaining() const { return (uint32_t)(size - pos); }

	int8_t ReadInt8() { int8_t t; ReadBytes(&t, 1); return t; }
	int16_t ReadInt16() { int16_t t; ReadBytes(&t, 2); return t; }
	int32_t ReadInt32() { int32_t t; ReadBytes(&t, 4); return t; }
	int64_t ReadInt64() { int64_t t; ReadBytes(&t, 8); return t; }
	float ReadFloat() { float t; ReadBytes(&t, 4); return t; }

	uint8_t ReadUInt8() { return ReadInt8(); }
	uint16_t ReadUInt16() { return ReadInt16(); }
	uint32_t ReadUInt32() { return ReadInt32(); }
	uint64_t ReadUInt64() { return ReadInt64(); }

	void Seek(uint32_t offset)
	{
		if (offset < startoffset)
			Exception::Throw("ObjectStream::Seek: Seeking outside object in" + package->GetPackageName().ToString());
		offset -= (uint32_t)startoffset;
		if (offset > size)
			Exception::Throw("ObjectStream::Seek: Unexpected end of file in " + package->GetPackageName().ToString());
		pos = offset;
	}

	void Skip(uint32_t bytes)
	{
		if (pos + bytes > size)
			Exception::Throw("ObjectStream::Skip: Unexpected end of file in " + package->GetPackageName().ToString());
		pos += bytes;
	}

	uint32_t Tell()
	{
		return (uint32_t)(startoffset + pos);
	}

	int32_t ReadIndex()
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

	std::string ReadString()
	{
		if (GetVersion() >= 64)
		{
			int len = ReadIndex();
			// A negative count is a string stored as UTF-16 (the original's
			// FString of wide characters: a conversation's comments, a note
			// typed past Latin-1): read as the engine's Latin-1, a character
			// past it a '?'
			if (len < 0)
			{
				if (len < -1024 * 1024)
					Exception::Throw("ObjectStream::ReadString: Invalid size in " + package->GetPackageName().ToString());
				std::string s;
				for (int i = 0; i < -len; i++)
				{
					uint16_t c = ReadUInt16();
					if (c == 0)
						continue;
					s.push_back(c < 256 ? (char)c : '?');
				}
				return s;
			}
			if (len > 1024 * 1024)
				Exception::Throw("ObjectStream::ReadString: Invalid size in " + package->GetPackageName().ToString());
			Array<char> s;
			s.resize(len);
			ReadBytes(s.data(), (int)s.size());
			s.push_back(0);
			return s.data();
		}
		else
		{
			return ReadAsciiZ();
		}
	}

	// A script's string, as the original's FString loads (Core.dll
	// 0x101459b0): a count with its terminator, negative for 16-bit
	// characters, else a byte each; the text ends at its terminator
	ScriptString ReadScriptString()
	{
		if (GetVersion() < 64)
			return Latin1ToScript(ReadAsciiZ());
		int len = ReadIndex();
		if (len < -1024 * 1024 || len > 1024 * 1024)
			Exception::Throw("ObjectStream::ReadScriptString: Invalid size in " + package->GetPackageName().ToString());
		ScriptString s;
		if (len < 0)
		{
			s.reserve(-len);
			for (int i = 0; i < -len; i++)
				s.push_back((char16_t)ReadUInt16());
		}
		else
		{
			s.reserve(len);
			for (int i = 0; i < len; i++)
				s.push_back((char16_t)ReadUInt8());
		}
		size_t end = s.find(u'\0');
		if (end != ScriptString::npos)
			s.resize(end);
		return s;
	}

	std::string ReadAsciiZ()
	{
		std::string s;
		while (true)
		{
			char c = ReadInt8();
			if (c == 0) break;
			s.push_back(c);
		}
		return s;
	}

	std::wstring ReadUnicodeZ()
	{
		std::wstring s;
		while (true)
		{
			wchar_t c = ReadUInt16();
			if (c == 0) break;
			s.push_back(c);
		}
		return s;
	}

	NameString ReadName()
	{
		return package->GetName(ReadIndex());
	}

	template<typename T>
	T* ReadObject();

	int GetVersion() const
	{
		return package->GetVersion();
	}

	UObject* GetUObject(int objref)
	{
		return package->GetUObject(objref);
	}

	const NameString& GetObjectName() const
	{
		return name;
	}

	UClass* GetObjectBase()
	{
		return base;
	}

	Package* GetPackage()
	{
		return package;
	}

private:
	Package* package = nullptr;
	std::unique_ptr<uint64_t[]> buffer;
	const uint8_t* data = nullptr;
	size_t startoffset = 0;
	size_t size = 0;
	size_t pos = 0;
	NameString name;
	UClass* base = nullptr;
};
