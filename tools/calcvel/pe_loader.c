// The game's Engine.dll and Core.dll mapped at their own bases in a 32-bit
// Linux process, so that a leaf of the original's x87 arithmetic can be run
// on the host's own x87 with inputs of the test's choosing. Engine's imports
// from Core bound by name; every other import, and Core's own, a trap.
// Nothing is written to the DLL files: they are read into memory.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include "pe_loader.h"

static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static void trap(void)
{
	fprintf(stderr, "an import the test does not bind was called\n");
	abort();
}

static const uint8_t* opt_header(uint32_t base)
{
	const uint8_t* img = (const uint8_t*)(uintptr_t)base;
	const uint8_t* pe = img + rd32(img + 0x3C);
	return pe + 24;
}

uint32_t pe_map(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) { perror(path); exit(1); }
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t* buf = malloc(size);
	if (fread(buf, 1, size, f) != (size_t)size) { perror("read"); exit(1); }
	fclose(f);

	const uint8_t* pe = buf + rd32(buf + 0x3C);
	if (memcmp(pe, "PE\0\0", 4) != 0) { fprintf(stderr, "%s: not PE\n", path); exit(1); }
	uint16_t nsec = rd16(pe + 6);
	uint16_t optsize = rd16(pe + 20);
	const uint8_t* opt = pe + 24;
	uint32_t imagebase = rd32(opt + 28);
	uint32_t sizeofimage = rd32(opt + 56);
	uint32_t sizeofheaders = rd32(opt + 60);

	void* at = mmap((void*)(uintptr_t)imagebase, sizeofimage, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
	if (at != (void*)(uintptr_t)imagebase) { fprintf(stderr, "%s: cannot map at %08x\n", path, imagebase); exit(1); }
	memcpy(at, buf, sizeofheaders);
	const uint8_t* sec = opt + optsize;
	for (int i = 0; i < nsec; i++, sec += 40)
	{
		uint32_t va = rd32(sec + 12), rawsize = rd32(sec + 16), rawptr = rd32(sec + 20);
		if (rawptr && rawsize)
			memcpy((uint8_t*)at + va, buf + rawptr, rawsize);
	}
	free(buf);
	return imagebase;
}

uint32_t pe_export(uint32_t base, const char* name)
{
	const uint8_t* img = (const uint8_t*)(uintptr_t)base;
	const uint8_t* opt = opt_header(base);
	uint32_t rva = rd32(opt + 96);
	const uint8_t* exp = img + rva;
	uint32_t nnames = rd32(exp + 24);
	const uint8_t* funcs = img + rd32(exp + 28);
	const uint8_t* names = img + rd32(exp + 32);
	const uint8_t* ords = img + rd32(exp + 36);
	for (uint32_t i = 0; i < nnames; i++)
	{
		const char* n = (const char*)img + rd32(names + 4 * i);
		if (strcmp(n, name) == 0)
			return base + rd32(funcs + 4 * rd16(ords + 2 * i));
	}
	return 0;
}

void pe_bind(uint32_t base, uint32_t coreBase)
{
	uint8_t* img = (uint8_t*)(uintptr_t)base;
	const uint8_t* opt = opt_header(base);
	uint32_t rva = rd32(opt + 104);
	for (uint8_t* d = img + rva; rd32(d + 12); d += 20)
	{
		const char* dll = (const char*)img + rd32(d + 12);
		uint32_t oft = rd32(d), ft = rd32(d + 16);
		uint8_t* names = img + (oft ? oft : ft);
		uint8_t* iat = img + ft;
		int core = coreBase && strcasecmp(dll, "Core.dll") == 0;
		for (int k = 0; rd32(names + 4 * k); k++)
		{
			uint32_t v = rd32(names + 4 * k);
			uint32_t addr = (uint32_t)(uintptr_t)trap;
			if (core && !(v & 0x80000000))
			{
				uint32_t a = pe_export(coreBase, (const char*)img + v + 2);
				if (a)
					addr = a;
			}
			memcpy(iat + 4 * k, &addr, 4);
		}
	}
}

// The function's structured-exception frame reads and writes fs:0, which a
// Linux process has no TIB behind: each such access is made a run of nops
// in the mapped copy (the frame is used only to unwind, which no input
// here reaches).
int pe_strip_seh(uint32_t start, uint32_t end)
{
	int n = 0;
	for (uint8_t* p = (uint8_t*)(uintptr_t)start; p + 7 <= (uint8_t*)(uintptr_t)end; p++)
	{
		static const uint8_t movEaxFs[6] = { 0x64, 0xA1, 0, 0, 0, 0 };
		static const uint8_t movFsEsp[7] = { 0x64, 0x89, 0x25, 0, 0, 0, 0 };
		static const uint8_t movFsEcx[7] = { 0x64, 0x89, 0x0D, 0, 0, 0, 0 };
		if (memcmp(p, movEaxFs, 6) == 0) { memset(p, 0x90, 6); n++; }
		else if (memcmp(p, movFsEsp, 7) == 0 || memcmp(p, movFsEcx, 7) == 0) { memset(p, 0x90, 7); n++; }
	}
	return n;
}
