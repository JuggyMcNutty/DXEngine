//=============================================================================
// OpsConsole: the natives' edges, logged with "DXOPS:" from wherever the run
// starts, 2 s in (docs/DEVELOPMENT.md, scripted runs): an int divided and
// multiplied by a float in place (by 0.0, past an int's range); the shifts
// by a count of 33 and of -1; a switch on a string against a case of the
// other case, and on -0.0 against 0.0; new with a flag outside the script's
// (the engine logs "new: Flags 04000000 not allowed"); each actor iterator
// given None for its class, counted; OrthoRotation of four frames; two rats
// spawned, the first destroyed and its nextPawn read; PlayBlendAnim with a
// slot of 256 (the engine logs the slot out of range); GetNextSkin with the
// server browser's prefix; and, in a PostRender with the medium font set
// (the console's canvas comes with none), StrLen under STY_None and under
// STY_Normal, TextSize, and DrawText and TextSize with no font (the engine
// logs "No font"; never StrLen, which asserts in the original). Then an
// exit.
//=============================================================================
class OpsConsole extends Console;

var float RunTime;
var int Phase;
var bool bCanvasDone;

function IntOps()
{
	local int I, N;

	I = 5;
	I /= 0.0;
	Log("DXOPS: 5 /= 0.0 -> " $ I);
	I = 7;
	I *= 3000000000.0;
	Log("DXOPS: 7 *= 3e9 -> " $ I);
	I = -7;
	I /= 0.5;
	Log("DXOPS: -7 /= 0.5 -> " $ I);
	I = 3;
	I *= 2.5;
	Log("DXOPS: 3 *= 2.5 -> " $ I);
	I = -3;
	I *= 2.5;
	Log("DXOPS: -3 *= 2.5 -> " $ I);
	I = 2147483647;
	I *= 2.0;
	Log("DXOPS: 2147483647 *= 2.0 -> " $ I);
	I = 100;
	I /= 7.0;
	Log("DXOPS: 100 /= 7.0 -> " $ I);
	N = 33;
	Log("DXOPS: 1 << 33 -> " $ (1 << N) $ ", -8 >> 33 -> " $ ((-8) >> N) $ ", -8 >>> 33 -> " $ ((-8) >>> N));
	N = -1;
	Log("DXOPS: 1 << -1 -> " $ (1 << N) $ ", -8 >> -1 -> " $ ((-8) >> N) $ ", -8 >>> -1 -> " $ ((-8) >>> N));
}

function SwitchOps()
{
	local string S;
	local float F;

	S = "abc";
	switch (S)
	{
		case "ABC":
			Log("DXOPS: switch on abc: case ABC matched");
			break;
		case "abc":
			Log("DXOPS: switch on abc: case abc matched");
			break;
		default:
			Log("DXOPS: switch on abc: no case matched");
	}
	F = 0.0;
	F = -F;
	switch (F)
	{
		case 0.0:
			Log("DXOPS: switch on -0.0: case 0.0 matched");
			break;
		default:
			Log("DXOPS: switch on -0.0: no case matched");
	}
}

function NewOps()
{
	local Object O;

	Log("DXOPS: new with the flag 0x04000000 follows");
	O = new(None, 'OpsObject', 0x04000000) class'Object';
	Log("DXOPS: new with the flag 0x04000000: made " $ (O != None));
}

function IterOps(PlayerPawn P)
{
	local Actor A;
	local class<Actor> C;
	local int n, i;

	C = None;
	n = 0;
	foreach P.AllActors(C, A)
		n++;
	Log("DXOPS: AllActors(None): " $ n);
	n = 0;
	foreach P.RadiusActors(C, A, 300)
		n++;
	Log("DXOPS: RadiusActors(None, 300): " $ n);
	n = 0;
	foreach P.BasedActors(C, A)
		n++;
	Log("DXOPS: BasedActors(None): " $ n);
	n = 0;
	foreach P.ChildActors(C, A)
		n++;
	Log("DXOPS: ChildActors(None): " $ n);
	n = 0;
	foreach P.TouchingActors(C, A)
		n++;
	Log("DXOPS: TouchingActors(None): " $ n);
	n = 0;
	foreach P.VisibleActors(C, A, 300)
		n++;
	Log("DXOPS: VisibleActors(None, 300): " $ n);
	n = 0;
	foreach P.VisibleCollidingActors(C, A, 300)
		n++;
	Log("DXOPS: VisibleCollidingActors(None, 300): " $ n);
	n = 0;
	foreach P.Region.Zone.ZoneActors(C, A)
		n++;
	Log("DXOPS: ZoneActors(None): " $ n);
	ZoneCensus(P);
	n = 0;
	i = 0;
	foreach P.CycleActors(C, A, i)
	{
		n++;
		if (n > 5000)
			break;
	}
	Log("DXOPS: CycleActors(None), one lap: " $ n);
}

// The player's zone's actors by kind, with the level's of each kind: where
// a zone count differs between the engines, which kind holds the actors
function ZoneCensus(PlayerPawn P)
{
	local Actor A;
	local Pawn Q;
	local int i, z, t;
	local class<Actor> Kinds[14];

	Kinds[0] = class'Pawn';
	Kinds[1] = class'Inventory';
	Kinds[2] = class'Decoration';
	Kinds[3] = class'Light';
	Kinds[4] = class'Info';
	Kinds[5] = class'Brush';
	Kinds[6] = class'Mover';
	Kinds[7] = class'Keypoint';
	Kinds[8] = class'NavigationPoint';
	Kinds[9] = class'Triggers';
	Kinds[10] = class'Projectile';
	Kinds[11] = class'Effects';
	Kinds[12] = class'Carcass';
	Kinds[13] = class'Decal';
	for (i = 0; i < 14; i++)
	{
		z = 0;
		t = 0;
		foreach P.Region.Zone.ZoneActors(Kinds[i], A)
			z++;
		foreach P.AllActors(Kinds[i], A)
			t++;
		Log("DXOPS: zone census " $ Kinds[i].Name $ ": " $ z $ " of " $ t);
	}
	// The animals, zone by zone: the pawns the census finds outside the
	// player's zone in one engine and not the other
	foreach P.AllActors(class'Pawn', Q)
		if (Q.IsA('Animal') || Q.Region.Zone != P.Region.Zone)
			Log("DXOPS: pawn " $ Q.Class.Name $ " in zone " $ Q.Region.ZoneNumber $ " (" $ Q.Region.Zone.Class.Name $ "), physics " $ Q.Physics $ ", in the player's zone " $ (Q.Region.Zone == P.Region.Zone));
}

function OrthoOps()
{
	local rotator R;

	R = OrthoRotation(vect(0,1,0), vect(-1,0,0), vect(0,0,1));
	Log("DXOPS: OrthoRotation(Y, -X, Z) = " $ R);
	R = OrthoRotation(vect(0.6,0,0.8), vect(0,1,0), vect(-0.8,0,0.6));
	Log("DXOPS: OrthoRotation pitched up = " $ R);
	R = OrthoRotation(vect(1,0,0), vect(0,0,1), vect(0,-1,0));
	Log("DXOPS: OrthoRotation rolled = " $ R);
	R = OrthoRotation(vect(0.36,0.48,0.8), vect(-0.8,0.6,0), vect(-0.48,-0.64,0.6));
	Log("DXOPS: OrthoRotation general = " $ R);
}

function PawnOps(PlayerPawn P)
{
	local Pawn A, B;
	local vector Spot;

	Spot = P.Location + vector(P.Rotation) * 150;
	A = P.Spawn(class'Rat',,, Spot + vect(0,0,10));
	B = P.Spawn(class'Rat',,, Spot + vect(60,0,10));
	if (A == None || B == None)
	{
		Log("DXOPS: no room for the rats at " $ Spot);
		return;
	}
	Log("DXOPS: the pawn list's head is the second rat: " $ (P.Level.PawnList == B) $ ", its next the first: " $ (B.nextPawn == A));
	A.Destroy();
	Log("DXOPS: the first rat destroyed: its nextPawn set " $ (A.nextPawn != None) $ ", the player " $ (A.nextPawn == P) $ "; the second's next is still it " $ (B.nextPawn == A));
	B.Destroy();
}

function BlendOps(PlayerPawn P)
{
	Log("DXOPS: PlayBlendAnim with slot 256 follows");
	P.PlayBlendAnim(P.AnimSequence,,, 256);
	Log("DXOPS: PlayBlendAnim with slot 256 done");
}

function SkinOps(Actor A)
{
	local string N, D;

	A.GetNextSkin("DeusExCharacters", "DeusExCharacters.Skins.JCDentonTex0", 0, N, D);
	Log("DXOPS: GetNextSkin(DeusExCharacters, JCDentonTex0, 0): '" $ N $ "' '" $ D $ "'");
	A.GetNextSkin("", "", 1, N, D);
	Log("DXOPS: GetNextSkin(empty, empty, 1): '" $ N $ "' '" $ D $ "'");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase == 0 && RunTime > 2.0)
	{
		IntOps();
		SwitchOps();
		NewOps();
		IterOps(P);
		OrthoOps();
		PawnOps(P);
		BlendOps(P);
		SkinOps(P);
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 3.5)
	{
		Log("DXOPS: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

event PostRender(canvas C)
{
	local Font F;
	local float XL, YL, XL2, YL2;

	Super.PostRender(C);
	if (bCanvasDone || Phase != 2)
		return;
	bCanvasDone = true;
	// The console's canvas comes with no Font (both engines), and the
	// original's StrLen asserts without one: the medium font is set first
	F = C.Font;
	C.Font = C.MedFont;
	C.Style = 0;
	C.StrLen("Hello world", XL, YL);
	C.Style = 1;
	C.StrLen("Hello world", XL2, YL2);
	Log("DXOPS: StrLen under STY_None " $ XL $ " by " $ YL $ ", under STY_Normal " $ XL2 $ " by " $ YL2);
	C.TextSize("Hello world", XL, YL);
	Log("DXOPS: TextSize " $ XL $ " by " $ YL);
	C.Font = None;
	Log("DXOPS: DrawText and TextSize with no font follow");
	C.DrawText("no font");
	XL = 1;
	YL = 2;
	C.TextSize("x", XL, YL);
	Log("DXOPS: TextSize with no font left " $ XL $ " by " $ YL);
	C.Font = F;
}
