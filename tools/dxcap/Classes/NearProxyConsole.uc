//=============================================================================
// NearProxyConsole (bugs.md D.68): whether an actor whose location lies
// within 1.5 units of the eye counts as drawn (LastRendered()), with a wall
// between that depth and 1.5 units.
//
// Render.dll sets an actor's occlusion proxy, its sprite's rectangle, back
// in the world at its location's own depth, however near (FDynamicSprite's
// constructor 0x10b233c0): its pieces go down the BSP by that depth and are
// tested against the span buffer where they come to rest in the walk. A
// mesh whose box holds the eye has the whole frame for its rectangle
// (BoundVisible). DXEngine held the proxy no nearer than 1.5 units
// (VisibleFrame::AddOcclusionProxy), so that one nearer than a wall 1.25
// units off lay behind it, under the wall's spans, and went undrawn.
//
// On TargetMap ([DXCapture.NearProxyConsole], 01_NYC_UNATCOIsland by
// default), the player out of the world, the HUD hidden, every pawn gone,
// conversations and datalinks stopped, 3 s in: of the level's path nodes,
// up to Walls (6) whose level line in one of eight ways meets a wall 64 to
// 400 units off (its normal within 0.3 of level), one wall a node. For each
// wall: the eye Gap (1.25) units off the wall's face, looking straight at
// it; bare meshes (CrateUnbreakableSmall's, collision off, unlit) at 0.05,
// 0.5, 1.0, 1.4 and 2.0 units ahead of the eye on its line to the wall, the
// first three in front of the wall, the last two behind it. 0.6 s held,
// then "DXNEARPROXY: wall w gap g depth d since L now T seen S" for each
// crate, L its LastRendered(), the time since it was last drawn, S true
// when that is under a tenth of a second.
//
// Both engines, on all six walls: the crates at 0.05 to 1.4 units seen, the
// one at 2 not; DXEngine's proxy held at 1.5 units gave the same, a proxy
// that joins the walk below the wall's node being tested on the eye's side
// whatever its depth (bugs.md REN.63).
//=============================================================================
class NearProxyConsole extends Console;

var config string TargetMap;
var config int Walls;
var config float Gap;

var float MapTime, StepTime;
var string CurrentMap;
var int Phase;
var bool bOut;
var vector Eye;
var rotator Look;
var Object Stopped;
var vector Face[8];
var vector Facing[8];
var int NumWalls;
var int Wall;
var Actor Crates[5];
var float Depths[5];

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;
	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function Hush(PlayerPawn P)
{
	local Actor A;

	foreach P.AllActors(class'Actor', A)
	{
		if (A == P)
			continue;
		if (A.IsA('Pawn') || A.IsA('SecurityCamera') || A.IsA('AutoTurret') || A.IsA('AlarmUnit') || A.IsA('LaserTrigger') || A.IsA('BeamTrigger'))
			A.Destroy();
	}
}

function Silence(PlayerPawn P)
{
	local DeusExPlayer DXP;

	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		DXP.conPlay.TerminateConversation();
	}
}

function Hold(PlayerPawn P)
{
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0,0,0);
}

// Up to Walls walls, one a path node: the first of eight level lines from
// the node's eye height that meets a wall 64 to 400 units off
function PickWalls(PlayerPawn P)
{
	local PathNode V;
	local vector E, Dir, HitLocation, HitNormal;
	local rotator R;
	local Actor HitActor;
	local int k, Tried;

	if (Walls <= 0 || Walls > 8)
		Walls = 6;
	foreach P.AllActors(class'PathNode', V)
	{
		if (NumWalls >= Walls || ++Tried > 200)
			break;
		E = V.Location;
		E.Z += P.BaseEyeHeight;
		for (k = 0; k < 8; k++)
		{
			R.Pitch = 0;
			R.Roll = 0;
			R.Yaw = k * 8192;
			Dir = vector(R);
			HitActor = P.Trace(HitLocation, HitNormal, E + Dir * 400, E, false);
			if (HitActor == None || Abs(HitNormal.Z) > 0.3 || VSize(HitLocation - E) < 64)
				continue;
			HitNormal.Z = 0;
			HitNormal = Normal(HitNormal);
			Face[NumWalls] = HitLocation;
			Facing[NumWalls] = HitNormal;
			Log("DXNEARPROXY: wall " $ NumWalls $ " from " $ V.Name $ " at " $ HitLocation $ " normal " $ HitNormal $ " hit " $ HitActor.Name);
			NumWalls++;
			break;
		}
	}
}

function Actor Crate(PlayerPawn P, vector Spot)
{
	local Actor C;

	C = P.Spawn(class'Effects',,, Spot, Look);
	if (C == None)
		return None;
	C.SetCollision(false, false, false);
	C.bCollideWorld = false;
	C.SetPhysics(PHYS_None);
	C.LifeSpan = 0;
	C.bHidden = false;
	C.DrawType = DT_Mesh;
	C.Style = STY_Normal;
	C.Mesh = class'CrateUnbreakableSmall'.default.Mesh;
	C.Skin = class'CrateUnbreakableSmall'.default.Skin;
	C.DrawScale = class'CrateUnbreakableSmall'.default.DrawScale;
	C.bUnlit = true;
	return C;
}

// The eye Gap units off wall K's face, looking at it, the crates on its line
function SetWall(PlayerPawn P, int K)
{
	local int i;

	for (i = 0; i < 5; i++)
	{
		if (Crates[i] != None)
			Crates[i].Destroy();
		Crates[i] = None;
	}
	Eye = Face[K] + Facing[K] * Gap;
	Look = rotator(-Facing[K]);
	Look.Pitch = 0;
	Look.Roll = 0;
	for (i = 0; i < 5; i++)
	{
		// SetLocation, as Spawn would move one in solid out of it
		Crates[i] = Crate(P, Eye);
		if (Crates[i] == None)
			Log("DXNEARPROXY: wall " $ K $ " depth " $ Depths[i] $ ": no crate");
		else
			Crates[i].SetLocation(Eye - Facing[K] * Depths[i]);
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local int i;
	local float Now;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	Silence(P);

	if (Phase == 0)
	{
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXNEARPROXY: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
			return;
		}
		if (!bOut && DeusExPlayer(P) != None)
		{
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		if (Gap <= 0)
			Gap = 1.25;
		Depths[0] = 0.05;
		Depths[1] = 0.5;
		Depths[2] = 1.0;
		Depths[3] = 1.4;
		Depths[4] = 2.0;
		Hush(P);
		DeusExPlayer(P).ShowHud(False);
		PickWalls(P);
		if (NumWalls == 0)
		{
			Log("DXNEARPROXY: no wall found, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Wall = 0;
		SetWall(P, Wall);
		Hold(P);
		StepTime = 0;
		Phase = 1;
		return;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= Target())
			Phase = 0;
		return;
	}

	Hold(P);
	if (Phase == 1 && StepTime > 0.6)
	{
		Now = P.Level.TimeSeconds;
		for (i = 0; i < 5; i++)
		{
			if (Crates[i] != None)
				Log("DXNEARPROXY: wall " $ Wall $ " gap " $ Gap $ " depth " $ Depths[i] $ " since " $ Crates[i].LastRendered()
				    $ " now " $ Now $ " seen " $ (Crates[i].LastRendered() < 0.1));
		}
		Wall++;
		if (Wall >= NumWalls)
		{
			Log("DXNEARPROXY: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		SetWall(P, Wall);
		Hold(P);
		StepTime = 0;
	}
}
