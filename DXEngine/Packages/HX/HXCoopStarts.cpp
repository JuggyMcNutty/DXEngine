#include "Precomp.h"
#include "HXCoopStarts.h"
#include "Utils/StrTools.h"
#include <cstring>

const char* const HXCoopStarts::DllSha1 = "0ecdafc3e5bfe7c5aef9abdd93c6a8520e62161e";

namespace
{
	// Where HX 0.9.89.4's HX.dll has the code read, as relative virtual
	// addresses (its image base is 0x10000000)
	const uint32_t SpawnCoopStarts = 0x25e20;		// HXSpawnCoopStarts(level, map), to its end
	const uint32_t SpawnCoopStartsEnd = 0x3a21e;
	const uint32_t SpawnSingleStart = 0x25cb0;		// HXSpawnSingleStart, the sixteen arguments below
	const uint32_t MapText = 0x3a220;				// the map string's text, no arguments
	const uint32_t MapLength = 0x14680;				// the map string's length, no arguments
	const uint32_t MapTextOf = 0xc130;				// the map string's text, one argument it takes off the stack
	const uint32_t MapFree = 0x1240;				// the map string's destructor, before the return

	// HXSpawnSingleStart's arguments, in their order (dx-reverse-info hx-dll.md)
	enum Argument
	{
		ArgLevel, ArgTag, ArgX, ArgY, ArgZ, ArgPitch, ArgYaw, ArgRoll, ArgPriority, ArgDefaultStart,
		ArgTeamNumber, ArgInitiallyEnabled, ArgEnableTag, ArgDisableTag, ArgEnabledMessage, ArgCollideActors,
		ArgCount
	};

	uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
	uint32_t U32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

	// A 32-bit PE image's sections, for the place of an address in the file
	struct Image
	{
		const uint8_t* data = nullptr;
		size_t size = 0;
		uint32_t imageBase = 0;
		size_t sections = 0;
		size_t numSections = 0;

		bool Has(size_t offset, size_t length) const { return offset <= size && length <= size - offset; }

		bool Open(const uint8_t* bytes, size_t length)
		{
			data = bytes;
			size = length;
			if (!Has(0, 0x40) || data[0] != 'M' || data[1] != 'Z')
				return false;
			size_t pe = U32(data + 0x3c);
			if (!Has(pe, 24) || std::memcmp(data + pe, "PE\0\0", 4) != 0 || U16(data + pe + 4) != 0x14c)
				return false;
			numSections = U16(data + pe + 6);
			size_t optional = pe + 24, optionalSize = U16(data + pe + 20);
			if (!Has(optional, 32) || U16(data + optional) != 0x10b)
				return false;
			imageBase = U32(data + optional + 28);
			sections = optional + optionalSize;
			return Has(sections, numSections * 40);
		}

		// The file's offset of an address relative to the image, its length
		// within one section's bytes
		bool Offset(uint32_t rva, size_t length, size_t& offset) const
		{
			for (size_t i = 0; i < numSections; i++)
			{
				const uint8_t* s = data + sections + i * 40;
				uint32_t va = U32(s + 12), rawSize = U32(s + 16), rawPointer = U32(s + 20);
				if (rva >= va && rva - va < rawSize && length <= rawSize - (rva - va))
				{
					offset = (size_t)rawPointer + (rva - va);
					return Has(offset, length);
				}
			}
			return false;
		}

		// A wide string at an address (the code's TEXT("...")), as UTF-8
		bool WideText(uint32_t address, std::string& text) const
		{
			text.clear();
			for (uint32_t i = 0; i < 1024; i++)
			{
				size_t offset;
				if (address < imageBase || !Offset(address - imageBase + i * 2, 2, offset))
					return false;
				uint16_t c = U16(data + offset);
				if (c == 0)
					return true;
				if (c < 0x80)
				{
					text += (char)c;
				}
				else if (c < 0x800)
				{
					text += (char)(0xc0 | (c >> 6));
					text += (char)(0x80 | (c & 0x3f));
				}
				else
				{
					text += (char)(0xe0 | (c >> 12));
					text += (char)(0x80 | ((c >> 6) & 0x3f));
					text += (char)(0x80 | (c & 0x3f));
				}
			}
			return false;
		}
	};

	// What a register or a stack slot holds, as far as the reader knows it
	enum class Kind { Unknown, Int, Map, Level, Import };
	struct Value
	{
		Kind kind = Kind::Unknown;
		uint32_t v = 0;
	};

	std::string Hex(uint32_t value)
	{
		char text[16];
		std::snprintf(text, sizeof(text), "0x%08x", value);
		return text;
	}
}

bool HXCoopStarts::Read(const Array<uint8_t>& dll, const std::string& map, Array<Start>& starts, std::string& error)
{
	starts.clear();
	Image image;
	size_t codeOffset;
	if (!image.Open(dll.data(), dll.size()))
	{
		error = "not a 32-bit PE image";
		return false;
	}
	if (!image.Offset(SpawnCoopStarts, SpawnCoopStartsEnd - SpawnCoopStarts, codeOffset))
	{
		error = "no HXSpawnCoopStarts at " + Hex(image.imageBase + SpawnCoopStarts);
		return false;
	}
	const uint8_t* code = image.data + codeOffset;

	// The registers by their number (eax, ecx, edx, ebx, esp, ebp, esi, edi),
	// the zero flag, the stack the pushes build; esp itself is not followed
	Value regs[8];
	bool zf = false;
	Array<Value> stack;
	uint32_t pc = SpawnCoopStarts;
	uint32_t mapLength = 0;
	for (size_t i = 0; i < map.size(); i++)
	{
		if (((uint8_t)map[i] & 0xc0) != 0x80)
			mapLength++;
	}

	auto fail = [&](const std::string& what) {
		error = what + " at " + Hex(image.imageBase + pc);
		starts.clear();
		return false;
	};
	// A string argument's text: the map's, or a constant's
	auto text = [&](const Value& value, std::string& result) {
		if (value.kind == Kind::Map)
		{
			result = map;
			return true;
		}
		return value.kind == Kind::Int && image.WideText(value.v, result);
	};
	// A call leaves eax, ecx and edx as the callee left them
	auto clobber = [&]() { regs[0] = regs[1] = regs[2] = Value(); };

	for (int steps = 0; steps < 1000000; steps++)
	{
		if (pc < SpawnCoopStarts || pc >= SpawnCoopStartsEnd)
			return fail("a jump out of HXSpawnCoopStarts");
		const uint8_t* p = code + (pc - SpawnCoopStarts);
		const size_t left = SpawnCoopStartsEnd - pc;
		const uint8_t op = p[0];
		auto need = [&](size_t length) { return length <= left; };
		auto modrm = [&]() { return need(2) ? p[1] : (uint8_t)0; };
		const bool registerForm = (modrm() & 0xc0) == 0xc0;
		const int reg = (modrm() >> 3) & 7, rm = modrm() & 7;

		if (op >= 0x50 && op <= 0x57)					// push r32
		{
			stack.push_back(regs[op - 0x50]);
			pc += 1;
		}
		else if (op >= 0x58 && op <= 0x5f)				// pop r32
		{
			if (stack.empty())
				return fail("a pop of an empty stack");
			regs[op - 0x58] = stack.back();
			stack.pop_back();
			pc += 1;
		}
		else if (op == 0x68 && need(5))					// push imm32
		{
			stack.push_back({ Kind::Int, U32(p + 1) });
			pc += 5;
		}
		else if (op == 0x6a && need(2))					// push imm8, sign-extended
		{
			stack.push_back({ Kind::Int, (uint32_t)(int32_t)(int8_t)p[1] });
			pc += 2;
		}
		else if ((op == 0x74 || op == 0x75) && need(2))	// jz, jnz rel8
		{
			bool take = (op == 0x74) == zf;
			int8_t rel = (int8_t)p[1];
			pc += 2;
			if (take)
				pc += rel;
		}
		else if (op == 0xeb && need(2))					// jmp rel8
		{
			pc += 2 + (int8_t)p[1];
		}
		else if (op == 0xe9 && need(5))					// jmp rel32
		{
			pc += 5 + U32(p + 1);
		}
		else if (op == 0x0f && need(6) && (p[1] == 0x84 || p[1] == 0x85))	// jz, jnz rel32
		{
			bool take = (p[1] == 0x84) == zf;
			uint32_t rel = U32(p + 2);
			pc += 6;
			if (take)
				pc += rel;
		}
		else if (op == 0x0f && need(3) && p[1] == 0x94 && (p[2] & 0xc0) == 0xc0 && (p[2] & 7) < 4)	// setz r8 (al, cl, dl, bl)
		{
			Value& r = regs[p[2] & 7];
			if (r.kind != Kind::Int)
				return fail("a setz of a register not known");
			r.v = (r.v & ~0xffu) | (zf ? 1u : 0u);
			pc += 3;
		}
		else if (op == 0x33 && registerForm && reg == rm)	// xor r32, itself
		{
			regs[reg] = { Kind::Int, 0 };
			zf = true;
			pc += 2;
		}
		else if (op == 0x85 && registerForm && reg == rm)	// test r32, itself
		{
			const Value& r = regs[reg];
			if (r.kind == Kind::Int)
				zf = r.v == 0;
			else if (r.kind == Kind::Map)
				zf = false;	// the text's address
			else
				return fail("a test of a register not known");
			pc += 2;
		}
		else if (op == 0x8b && registerForm)			// mov r32, r32
		{
			if (reg != 4)	// mov esp, ebp: the return's alone
				regs[reg] = regs[rm];
			pc += 2;
		}
		else if (op == 0x8b && need(3) && (modrm() & 0xc7) == 0x45)	// mov r32, [ebp+disp8]
		{
			// [ebp+8] is the first argument, the level
			regs[reg] = p[2] == 8 ? Value{ Kind::Level, 0 } : Value();
			pc += 3;
		}
		else if (op == 0x8b && need(6) && (modrm() & 0xc7) == 0x05)	// mov r32, [disp32]: an import (appStricmp)
		{
			regs[reg] = { Kind::Import, U32(p + 2) };
			pc += 6;
		}
		else if (op == 0x89 && need(3) && (modrm() & 0xc7) == 0x45)	// mov [ebp+disp8], r32: the frame's own
		{
			pc += 3;
		}
		else if (op == 0x8d && need(3) && (modrm() & 0xc7) == 0x45)	// lea r32, [ebp+disp8]
		{
			regs[reg] = Value();
			pc += 3;
		}
		else if (op >= 0xb8 && op <= 0xbf && need(5))	// mov r32, imm32
		{
			regs[op - 0xb8] = { Kind::Int, U32(p + 1) };
			pc += 5;
		}
		else if (op == 0x64 && need(6) && p[1] == 0xa1)	// mov eax, fs:[disp32]: the exception frame
		{
			regs[0] = Value();
			pc += 6;
		}
		else if (op == 0x64 && need(7) && p[1] == 0x89 && (p[2] & 0xc7) == 0x05)	// mov fs:[disp32], r32
		{
			pc += 7;
		}
		else if (op == 0x83 && need(3) && (p[1] == 0xc4 || p[1] == 0xec) && p[2] < 0x80 && p[2] % 4 == 0)	// add, sub esp, imm8
		{
			for (int i = 0; i < p[2] / 4; i++)
			{
				if (p[1] == 0xec)
				{
					stack.push_back(Value());
				}
				else
				{
					if (stack.empty())
						return fail("a pop of an empty stack");
					stack.pop_back();
				}
			}
			pc += 3;
		}
		else if (op == 0xc6 && need(4) && p[1] == 0x45)	// mov byte [ebp+disp8], imm8
		{
			pc += 4;
		}
		else if (op == 0xc7 && need(7) && p[1] == 0x45)	// mov dword [ebp+disp8], imm32
		{
			pc += 7;
		}
		else if (op == 0xff && need(2) && p[1] == 0xd6)	// call esi: appStricmp(a, b), cdecl
		{
			std::string a, b;
			if (regs[6].kind != Kind::Import || stack.size() < 2)
				return fail("a call through esi not known");
			if (!text(stack[stack.size() - 1], a) || !text(stack[stack.size() - 2], b))
				return fail("a name test of a text not known");
			clobber();
			regs[0] = { Kind::Int, StrTools::equals_ignore_case(a, b) ? 0u : 1u };
			pc += 2;
		}
		else if (op == 0xe8 && need(5))					// call rel32
		{
			uint32_t target = pc + 5 + U32(p + 1);
			if (target == SpawnSingleStart)
			{
				// cdecl: the arguments stay for the caller's add esp
				if (stack.size() < ArgCount)
					return fail("a start's call with too few arguments");
				Value args[ArgCount];
				for (int i = 0; i < ArgCount; i++)
				{
					args[i] = stack[stack.size() - 1 - i];
					if (i != ArgLevel && args[i].kind != Kind::Int)
						return fail("a start's argument not known");
				}
				if (args[ArgLevel].kind != Kind::Level)
					return fail("a start's call for another level");
				Start start;
				float x, y, z;
				std::memcpy(&x, &args[ArgX].v, 4);
				std::memcpy(&y, &args[ArgY].v, 4);
				std::memcpy(&z, &args[ArgZ].v, 4);
				start.location = vec3(x, y, z);
				start.rotation = Rotator((int)args[ArgPitch].v, (int)args[ArgYaw].v, (int)args[ArgRoll].v);
				start.priority = (int)args[ArgPriority].v;
				start.defaultStart = (args[ArgDefaultStart].v & 1) != 0;
				start.teamNumber = (uint8_t)args[ArgTeamNumber].v;
				start.initiallyEnabled = (args[ArgInitiallyEnabled].v & 1) != 0;
				start.collideActors = args[ArgCollideActors].v != 0;
				if (!image.WideText(args[ArgTag].v, start.tag) || !image.WideText(args[ArgEnableTag].v, start.enableTag) ||
					!image.WideText(args[ArgDisableTag].v, start.disableTag) || !image.WideText(args[ArgEnabledMessage].v, start.enabledMessage))
					return fail("a start's text not found");
				starts.push_back(std::move(start));
				clobber();
			}
			else if (target == MapText)
			{
				clobber();
				regs[0] = { Kind::Map, 0 };
			}
			else if (target == MapTextOf)
			{
				if (stack.empty())
					return fail("a pop of an empty stack");
				stack.pop_back();
				clobber();
				regs[0] = { Kind::Map, 0 };
			}
			else if (target == MapLength)
			{
				clobber();
				regs[0] = { Kind::Int, mapLength };
			}
			else if (target == MapFree)
			{
				clobber();
			}
			else
			{
				return fail("a call of " + Hex(image.imageBase + target) + " not known");
			}
			pc += 5;
		}
		else if (op == 0xc3)							// ret: the end
		{
			return true;
		}
		else
		{
			return fail("an instruction not read");
		}
	}
	return fail("no end within a million instructions");
}
