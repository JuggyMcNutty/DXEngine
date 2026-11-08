// calcvel: the original's APawn::calcVelocity (Engine.dll 0x103cd7a0), its
// own instructions run on the host's x87, with the Core.dll functions it
// calls (FVector's constructor, SizeSquared, Size, SafeNormal, its
// operators, appSqrt, IsA), against DXEngine's UPawn::DeusExCalcVelocity
// as its source has it, on the same inputs: walking, braking, a walking
// player alone and in a net game, swimming, flying and edge cases (ticks of
// 0, 1e-6 and multiples of 0.03, speeds of 1e6). The velocity and
// acceleration after must be the same to the bit; the run fails otherwise.
// The x87 at 64 bits, as in the level's tick where physics runs; -single
// sets 24, as in the console's tick, and DXEngine's X87::Single with it.
// The game's DLLs are read, never written: build.sh, then
//   calcvel_test <gamefiles/System> [cases] [-single]
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pe_loader.h"

typedef struct
{
	float vel[3], acc[3];
	float mass, buoyancy, desiredSpeed, accelRate;
	float gravity[3];
	int player, walking, netMode;
} PawnState;

void dxe_accel_dir(const float a[3], float out[3]);
void dxe_draft_single(int on);
void dxe_draft(PawnState* s, const float d[3], float dt, float maxSpeed, float friction, int fluid, int brake, int buoyant);

typedef void (__attribute__((thiscall)) *CalcVelocityFn)(void* self, float dx, float dy, float dz, float dt, float maxSpeed, float friction, int fluid, int brake, int buoyant);

static CalcVelocityFn calc;
static uint32_t clsPlayerPawn, clsLevelInfo;
static uint8_t fakePawnClass[0x40];
static uint8_t pawn[0x1000], level[0x100], levelInfo[0x600], zone[0x400];
static uint32_t actors[4];

static void put32(uint8_t* p, uint32_t v) { memcpy(p, &v, 4); }
static void putf(uint8_t* p, float v) { memcpy(p, &v, 4); }

static void run_original(PawnState* s, const float d[3], float dt, float maxSpeed, float friction, int fluid, int brake, int buoyant)
{
	memset(pawn, 0, sizeof(pawn));
	memset(level, 0, sizeof(level));
	memset(levelInfo, 0, sizeof(levelInfo));
	memset(zone, 0, sizeof(zone));
	put32(pawn + 0x24, s->player ? clsPlayerPawn : (uint32_t)(uintptr_t)fakePawnClass);
	put32(pawn + 0xAC, (uint32_t)(uintptr_t)level);
	put32(level + 0x2C, (uint32_t)(uintptr_t)actors);
	actors[0] = (uint32_t)(uintptr_t)levelInfo;
	put32(levelInfo + 0x24, clsLevelInfo);
	levelInfo[0x518] = (uint8_t)s->netMode;
	put32(pawn + 0xC8, (uint32_t)(uintptr_t)zone);
	memcpy(zone + 0x31C, s->gravity, 12);
	memcpy(pawn + 0x144, s->vel, 12);
	memcpy(pawn + 0x150, s->acc, 12);
	putf(pawn + 0x1F4, s->mass);
	putf(pawn + 0x1F8, s->buoyancy);
	pawn[0x318] = s->walking ? 0x10 : 0;
	putf(pawn + 0x36C, s->desiredSpeed);
	putf(pawn + 0x384, s->accelRate);
	calc(pawn, d[0], d[1], d[2], dt, maxSpeed, friction, fluid, brake, buoyant);
	memcpy(s->vel, pawn + 0x144, 12);
	memcpy(s->acc, pawn + 0x150, 12);
}

// Deterministic inputs: a 64-bit LCG, floats made by exact steps
static uint64_t rng = 0x9e3779b97f4a7c15ull;
static uint32_t next32(void) { rng = rng * 6364136223846793005ull + 1442695040888963407ull; return (uint32_t)(rng >> 32); }
static float uni(float lo, float hi) { return lo + (hi - lo) * (float)((next32() >> 8) * (1.0 / 16777216.0)); }
static int pick(int n) { return (int)(next32() % (uint32_t)n); }

static int same(const float* a, const float* b, int n) { return memcmp(a, b, 4 * n) == 0; }

typedef struct { const char* name; int cases, shown, fileVel, fileAcc; } Tally;

static void report(Tally* t)
{
	printf("%-28s %7d cases: velocity apart %d, acceleration apart %d\n", t->name, t->cases, t->fileVel, t->fileAcc);
}

static void one(Tally* t, PawnState base, float dt, float maxSpeed, float friction, int fluid, int brake, int buoyant)
{
	float d[3];
	dxe_accel_dir(base.acc, d);
	PawnState o = base, n = base;
	run_original(&o, d, dt, maxSpeed, friction, fluid, brake, buoyant);
	dxe_draft(&n, d, dt, maxSpeed, friction, fluid, brake, buoyant);
	t->cases++;
	int nv = !same(o.vel, n.vel, 3), na = !same(o.acc, n.acc, 3);
	t->fileVel += nv;
	t->fileAcc += na;
	if ((nv || na) && t->shown < 5)
	{
		t->shown++;
		printf("  %s: dt %.9g max %.9g fr %.9g fluid %d brake %d buoy %d player %d walking %d net %d\n",
			t->name, dt, maxSpeed, friction, fluid, brake, buoyant, base.player, base.walking, base.netMode);
		printf("    in  v (%.9g %.9g %.9g) a (%.9g %.9g %.9g)\n", base.vel[0], base.vel[1], base.vel[2], base.acc[0], base.acc[1], base.acc[2]);
		printf("    orig v (%.9g %.9g %.9g) a (%.9g %.9g %.9g)\n", o.vel[0], o.vel[1], o.vel[2], o.acc[0], o.acc[1], o.acc[2]);
		printf("    dxe  v (%.9g %.9g %.9g) a (%.9g %.9g %.9g)\n", n.vel[0], n.vel[1], n.vel[2], n.acc[0], n.acc[1], n.acc[2]);
	}
}

static float tick(void)
{
	// frame times: 60 to 1000 fps, round 30, 20, 10 fps, near multiples of 0.03, a long frame
	switch (pick(6))
	{
	case 0: return uni(0.001f, 0.0167f);
	case 1: return 1.0f / (float)(20 + pick(130));
	case 2: return 0.03f * (float)(1 + pick(4)) + uni(-1e-6f, 1e-6f);
	case 3: return uni(0.0167f, 0.1f);
	case 4: return uni(0.1f, 0.5f);
	default: return 1.0f / 60.0f;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <gamefiles/System> [cases] [-single]\n", argv[0]);
		return 1;
	}
	int cases = argc > 2 ? atoi(argv[2]) : 20000;
	int single = argc > 3 && strcmp(argv[3], "-single") == 0;
	char path[4096];
	snprintf(path, sizeof(path), "%s/Core.dll", argv[1]);
	uint32_t core = pe_map(path);
	snprintf(path, sizeof(path), "%s/Engine.dll", argv[1]);
	uint32_t engine = pe_map(path);
	pe_bind(core, 0);
	pe_bind(engine, core);
	uint32_t fn = pe_export(engine, "?calcVelocity@APawn@@AAEXVFVector@@MMMHHH@Z");
	clsPlayerPawn = pe_export(engine, "?PrivateStaticClass@APlayerPawn@@0VUClass@@A");
	clsLevelInfo = pe_export(engine, "?PrivateStaticClass@ALevelInfo@@0VUClass@@A");
	if (!fn || !clsPlayerPawn || !clsLevelInfo)
	{
		fprintf(stderr, "exports not found\n");
		return 1;
	}
	// The export may be the incremental linker's jump: follow it
	uint8_t* p = (uint8_t*)(uintptr_t)fn;
	if (p[0] == 0xE9)
	{
		int32_t rel;
		memcpy(&rel, p + 1, 4);
		fn = fn + 5 + rel;
	}
	printf("calcVelocity at %08x, %d fs:0 accesses made nops\n", fn, pe_strip_seh(fn, fn + 0xa5a));
	calc = (CalcVelocityFn)(uintptr_t)fn;

	unsigned short cw;
	__asm__ volatile("fnstcw %0" : "=m"(cw));
	if (single)
	{
		unsigned short c = (unsigned short)(cw & ~0x0300);
		__asm__ volatile("fldcw %0" : : "m"(c));
		dxe_draft_single(1);
	}
	__asm__ volatile("fnstcw %0" : "=m"(cw));
	printf("x87 control word %04x (precision %s)\n", cw, (cw & 0x0300) == 0x0300 ? "64 bits" : (cw & 0x0300) == 0 ? "24 bits" : "53 bits");

	Tally walk = { "walking (friction 8, 2048)" }, walkAny = { "walking, any rates" }, brake = { "walking, braking" },
		player = { "walking player" }, swim = { "swimming" }, fly = { "flying" }, edges = { "edges" };
	for (int i = 0; i < cases; i++)
	{
		PawnState s;
		memset(&s, 0, sizeof(s));
		s.mass = 100.0f;
		s.desiredSpeed = 1.0f;
		s.gravity[2] = -950.0f;

		// Walking as Deus Ex's NPCs walk: the stock zone's friction 8, AccelRate 2048
		float a = uni(0.0f, 6.2831853f);
		float sp = uni(0.0f, 400.0f);
		s.vel[0] = sp * (float)__builtin_cos(a); s.vel[1] = sp * (float)__builtin_sin(a);
		float b = a + uni(-3.0f, 3.0f);
		float ar = 2048.0f;
		s.accelRate = ar;
		s.acc[0] = ar * (float)__builtin_cos(b); s.acc[1] = ar * (float)__builtin_sin(b);
		s.desiredSpeed = uni(0.1f, 1.0f);
		one(&walk, s, tick(), uni(150.0f, 400.0f), 8.0f, 0, 1, 0);

		// any AccelRate, friction and acceleration's size
		s.accelRate = uni(100.0f, 4000.0f);
		float am = uni(0.0f, 5000.0f);
		s.acc[0] = am * (float)__builtin_cos(b); s.acc[1] = am * (float)__builtin_sin(b);
		one(&walkAny, s, tick(), uni(50.0f, 600.0f), uni(0.1f, 10.0f), 0, 1, 0);

		// braking: no acceleration
		s.acc[0] = s.acc[1] = s.acc[2] = 0.0f;
		one(&brake, s, tick(), uni(50.0f, 600.0f), pick(2) ? 8.0f : uni(0.1f, 10.0f), 0, 1, 0);

		// the player, walking or not, alone or in a net game
		s.player = 1;
		s.walking = pick(2);
		s.netMode = pick(2) ? 0 : 1 + pick(3);
		s.accelRate = pick(2) ? 2048.0f : uni(500.0f, 3000.0f);
		if (pick(4))
		{
			s.acc[0] = s.accelRate * (float)__builtin_cos(b); s.acc[1] = s.accelRate * (float)__builtin_sin(b);
		}
		one(&player, s, tick(), pick(2) ? 320.0f : uni(100.0f, 600.0f), pick(2) ? 8.0f : uni(0.1f, 10.0f), 0, 1, 0);

		// swimming: the zone's fluid friction, as a fluid, buoyant, in three dimensions
		s.player = pick(2);
		s.walking = s.player;
		s.netMode = 0;
		s.vel[2] = uni(-300.0f, 300.0f);
		s.acc[0] = uni(-2048.0f, 2048.0f); s.acc[1] = uni(-2048.0f, 2048.0f); s.acc[2] = uni(-2048.0f, 2048.0f);
		if (!pick(5))
			s.acc[0] = s.acc[1] = s.acc[2] = 0.0f;
		s.accelRate = pick(2) ? 2048.0f : uni(300.0f, 3000.0f);
		s.mass = uni(1.0f, 300.0f);
		s.buoyancy = uni(0.0f, s.mass * 1.2f);
		one(&swim, s, tick(), uni(100.0f, 400.0f), uni(0.0f, 4.0f), 1, 0, 1);

		// flying: as a fluid, not buoyant
		s.player = 0;
		s.walking = 0;
		one(&fly, s, tick(), uni(100.0f, 800.0f), uni(0.0f, 4.0f), 1, 0, 0);

		// edges: tiny and zero ticks, huge and zero speeds, no friction or
		// AccelRate, a tick of exactly 0.03, 0.06 and 0.09
		PawnState e = s;
		float scale = pick(3) == 0 ? 1e6f : pick(2) ? 1.0f : 1e-3f;
		e.vel[0] *= scale; e.vel[1] *= scale; e.vel[2] *= pick(2) ? scale : 0.0f;
		if (pick(3) == 0)
			e.acc[0] = e.acc[1] = e.acc[2] = 0.0f;
		if (pick(4) == 0)
			e.accelRate = 0.0f;
		e.player = pick(2);
		e.walking = pick(2);
		e.netMode = pick(2);
		float dts[6] = { 1e-6f, 1e-4f, 0.03f, 0.06f, 0.09f, 0.0f };
		float edt = dts[pick(6)];
		int mode = pick(3);
		one(&edges, e, edt, uni(0.0f, 1000.0f), pick(3) == 0 ? 0.0f : uni(0.0f, 10.0f), mode != 0, mode == 0, mode == 1);
	}
	Tally* all[] = { &walk, &walkAny, &brake, &player, &swim, &fly, &edges };
	int apart = 0;
	for (int i = 0; i < 7; i++)
	{
		report(all[i]);
		apart += all[i]->fileVel + all[i]->fileAcc;
	}
	printf("%s\n", apart ? "FAILED: DXEngine's calcVelocity is not the original's" : "alike to the bit");
	return apart ? 1 : 0;
}
