//=============================================================================
// Nat6Console: a probe of the natives' edges, logged with
// "DXNAT6:", 2 s in (DXEngine docs/DEVELOPMENT.md, scripted runs), then an
// exit.
//
// Safe cases, one function each, every run:
//  - VecDivOps: a vector over a float and /= a float, values whose quotient
//    and product by the reciprocal part at a float's last bit (100/3 is
//    33.333332 divided, 33.333336 times 1/3 at 24 bits);
//  - CrossOps, MirrorOps: Cross of products that are not exact in a float,
//    MirrorVectorByNormal with a normal of length 2 (normalised first:
//    (1,0,1); as given: (1,0,7));
//  - FloatZeroOps: float / 0, % 0, a vector / 0, and whether the original
//    logs anything for them (look at the lines just above in its log);
//  - MathEdgeOps: Sqrt(-1), Loge(0), Loge(-1), Exp(1000);
//  - RandOps: Rand(0), Rand(-5), Rand(1) a thousand times each (the counts
//    of non-zero answers);
//  - EnumOps: GetEnum past both ends;
//  - LoadOps: DynamicLoadObject with no class.
//
// One crash case a run, by config (DXCAP_CONFIG="CrashCase=<name>"), each
// expected to stop DXEngine (a null pointer): run each on its own, on the
// original first, and note what its log says just before:
//  - SoundNone:      GetSoundDuration(None) (engine-dll.md: no None check)
//  - DrawActorNone:  Canvas.DrawActor(None, False, True) in a PostRender
//                    (UT's idiom to clear the Z buffer)
//  - ClippedNone:    Canvas.DrawClippedActor(None, False, 64, 64, 0, 0, True)
//  - ConvertNone:    the root window's ConvertCoordinates(None, ...)
//=============================================================================
class Nat6Console extends Console;

var config string CrashCase;
var float RunTime;
var int Phase;
var bool bCanvasDone;

function VecDivOps()
{
	local vector V;

	V = vect(100,10,5) / 3.0;
	Log("DXNAT6: vect(100,10,5) / 3.0 -> " $ V);
	V = vect(9,7,123) / 7.0;
	Log("DXNAT6: vect(9,7,123) / 7.0 -> " $ V);
	V = vect(100,7,5);
	V /= 6.0;
	Log("DXNAT6: vect(100,7,5) /= 6.0 -> " $ V);
	V = vect(9,100,7) / 0.3;
	Log("DXNAT6: vect(9,100,7) / 0.3 -> " $ V);
	// The scalar forms beside them, for comparison
	Log("DXNAT6: 100.0 / 3.0 -> " $ (100.0 / 3.0) $ ", 100.0 * (1.0 / 3.0) -> " $ (100.0 * (1.0 / 3.0)));
}

function CrossOps()
{
	local vector C;

	C = vect(0.1,0.7,0.3) Cross vect(0.3,0.1,0.7);
	Log("DXNAT6: Cross((0.1,0.7,0.3),(0.3,0.1,0.7)) -> " $ C $ " ; x-0.46 e8 " $ ((C.X - 0.46) * 100000000.0) $ " ; y-0.02 e8 " $ ((C.Y - 0.02) * 100000000.0) $ " ; z+0.2 e8 " $ ((C.Z + 0.2) * 100000000.0));
	C = vect(1.1,2.3,3.7) Cross vect(1.1000001,2.3000002,3.7000003);
	Log("DXNAT6: Cross of near-parallel -> " $ (C * 100000000.0) $ " (x1e8)");
}

function MirrorOps()
{
	Log("DXNAT6: MirrorVectorByNormal((1,0,-1), (0,0,2)) -> " $ MirrorVectorByNormal(vect(1,0,-1), vect(0,0,2)));
	Log("DXNAT6: MirrorVectorByNormal((1,0,-1), (0,0,0)) -> " $ MirrorVectorByNormal(vect(1,0,-1), vect(0,0,0)));
	Log("DXNAT6: MirrorVectorByNormal((0.3,0.4,-0.866), (0,0,1)) -> " $ MirrorVectorByNormal(vect(0.3,0.4,-0.866), vect(0,0,1)));
}

function FloatZeroOps()
{
	local float Zero, F;
	local vector V;

	Zero = 0.0;
	Log("DXNAT6: float / 0 follows");
	F = 1.0 / Zero;
	Log("DXNAT6: 1.0 / 0 -> " $ F);
	F = -1.0 / Zero;
	Log("DXNAT6: -1.0 / 0 -> " $ F);
	F = 1.0;
	F /= Zero;
	Log("DXNAT6: 1.0 /= 0 -> " $ F);
	F = 5.0 % Zero;
	Log("DXNAT6: 5.0 % 0 -> " $ F);
	V = vect(1,0,-2) / Zero;
	Log("DXNAT6: vect(1,0,-2) / 0 -> " $ V);
	V = vect(1,0,-2);
	V /= Zero;
	Log("DXNAT6: vect(1,0,-2) /= 0 -> " $ V);
}

function MathEdgeOps()
{
	local float M;

	M = -1.0;
	Log("DXNAT6: Sqrt(-1) -> " $ Sqrt(M));
	Log("DXNAT6: Loge(0) -> " $ Loge(0.0) $ ", Loge(-1) -> " $ Loge(M));
	Log("DXNAT6: Exp(1000) -> " $ Exp(1000.0));
	Log("DXNAT6: Atan(1e30) -> " $ Atan(1000000000000000000000000000000.0));
}

function RandOps()
{
	local int I, N0, N1, N2, Lo, Hi;

	Lo = 0;
	Hi = 0;
	for (I = 0; I < 1000; I++)
	{
		if (Rand(0) != 0)
			N0++;
		if (Rand(-5) != 0)
			N1++;
		if (Rand(1) != 0)
			N2++;
	}
	Log("DXNAT6: non-zero answers of 1000: Rand(0) " $ N0 $ ", Rand(-5) " $ N1 $ ", Rand(1) " $ N2);
}

function EnumOps()
{
	Log("DXNAT6: GetEnum(EPhysics, 9999) -> '" $ GetEnum(enum'EPhysics', 9999) $ "', GetEnum(EPhysics, -1) -> '" $ GetEnum(enum'EPhysics', -1) $ "'");
}

function LoadOps()
{
	local Object O;

	O = DynamicLoadObject("DeusExSounds.Generic.Beep4", None, True);
	Log("DXNAT6: DynamicLoadObject(Beep4, None) -> " $ O);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float X, Y;
	local CapNat6Probe Probe;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase == 0 && RunTime > 2.0)
	{
		VecDivOps();
		CrossOps();
		MirrorOps();
		// The cases that rest on the sums' order and precision, here in the
		// console's tick and by the probe in the level's
		Probe = P.Spawn(class'CapNat6Probe');
		if (Probe != None)
			Probe.Cases("console's tick:");
		FloatZeroOps();
		MathEdgeOps();
		RandOps();
		EnumOps();
		// The original stops here with an assertion (UnObj.cpp 2183, no
		// class): a crash case of its own
		if (CrashCase ~= "LoadNone")
			LoadOps();
		Log("DXNAT6: safe cases done; crash case '" $ CrashCase $ "'");
		if (CrashCase ~= "SoundNone")
		{
			Log("DXNAT6: GetSoundDuration(None) follows");
			Log("DXNAT6: GetSoundDuration(None) -> " $ P.GetSoundDuration(None));
		}
		else if (CrashCase ~= "ConvertNone" && PlayerPawnExt(P) != None && PlayerPawnExt(P).rootWindow != None)
		{
			Log("DXNAT6: ConvertCoordinates(None, ...) follows");
			X = -1;
			Y = -1;
			PlayerPawnExt(P).rootWindow.ConvertCoordinates(None, 10, 20, PlayerPawnExt(P).rootWindow, X, Y);
			Log("DXNAT6: ConvertCoordinates(None, 10, 20, root) -> " $ X $ ", " $ Y);
		}
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 3.5)
	{
		Log("DXNAT6: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

event PostRender(canvas C)
{
	Super.PostRender(C);
	if (bCanvasDone || Phase != 2)
		return;
	bCanvasDone = true;
	if (CrashCase ~= "DrawActorNone")
	{
		Log("DXNAT6: Canvas.DrawActor(None, False, True) follows");
		C.DrawActor(None, False, True);
		Log("DXNAT6: Canvas.DrawActor(None, False, True) returned");
	}
	else if (CrashCase ~= "ClippedNone")
	{
		Log("DXNAT6: Canvas.DrawClippedActor(None, ...) follows");
		C.DrawClippedActor(None, False, 64, 64, 0, 0, True);
		Log("DXNAT6: Canvas.DrawClippedActor(None, ...) returned");
	}
}
