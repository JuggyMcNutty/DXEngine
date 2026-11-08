//=============================================================================
// FiledPlaneConsole (bugs.md PHYS.37): where a line meets a door
// filed in the level's BSP at a yaw off the quarter turns, logged with
// "DXFILED:" -- TargetMap ([DXCapture.FiledPlaneConsole], 02_NYC_BatteryPark
// by default) opened from wherever the run starts; 2 s into the level's time
// DoorName (DeusExMover47, Tag MainDoors: its brush 8 by 80 by 206, PrePivot
// at a corner of its hinge side, BaseRot yaw -16384, KeyRot[1] yaw 26624, so
// open at yaw 10240) opened with DoOpen; once it has stopped and a second
// more has passed (the render files it again where it stands), the door
// made not to collide, so that a line meets only its filed polygons (the
// level's hit); then lines 24 units long into two of its faces, each at
// five heights across the brush (local Z -80 to 80) and three depths
// across its thickness or width, from 16 units off the face along its
// normal: the free edge (local +Y, whose filed plane's W comes out on
// another float in DXEngine's order) and the broad face (local +X, alike in
// both by the reading). Each line's hit, its place and normal, as Trace
// gives them.
//
// The original files each corner as FPoly::Transform does (Engine.dll
// 0x10387360, from the tracker's filing 0x1037a400, at the render's 24
// bits): the corner less PrePivot, per axis, dotted with the coords' axes,
// plus Location. DXEngine's tracker (Collision/TopLevel/BrushTracker.cpp
// File) puts each corner through one float matrix T(Location) R T(-PrePivot),
// its last column worked out first. filedfloats.py, both orders in floats:
// for this door open, the edge's W -329.473114 (original) against
// -329.473022 (DXEngine), the broad faces' alike. Expected: the edge's 15
// lines' places along the normal some 9e-5 apart (the 4th or 5th decimal),
// the broad face's 15 alike.
//=============================================================================
class FiledPlaneConsole extends Console;

var config string TargetMap;
var config string DoorName;

var float MapTime;
var float StepTime;
var int Step;
var string CurrentMap;
var DeusExMover Door;

function Say(string S)
{
	Log("DXFILED: " $ S);
}

function string Target()
{
	if (TargetMap == "")
		return "02_NYC_BatteryPark";
	return TargetMap;
}

function string DoorWanted()
{
	if (DoorName == "")
		return "DeusExMover47";
	return DoorName;
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

// A point of the door's brush in the world, as both engines place it
function vector World(vector Local)
{
	return Door.Location + ((Local - Door.PrePivot) >> Door.Rotation);
}

// Lines 24 units long into a face: from 16 off it along its normal
function Face(PlayerPawn P, string What, vector LocalNormal, float FaceAt, vector Across)
{
	local int h, d;
	local vector Spot, N, Start, End, HitLoc, HitNorm;
	local Actor Hit;

	N = LocalNormal >> Door.Rotation;
	for (h = -2; h <= 2; h++)
	{
		for (d = -1; d <= 1; d++)
		{
			Spot = LocalNormal * FaceAt + Across * d;
			Spot.Z = 40 * h;
			Spot = World(Spot);
			Start = Spot + N * 16;
			End = Spot - N * 8;
			Hit = P.Trace(HitLoc, HitNorm, End, Start, true);
			Say(What $ " h " $ h $ " d " $ d $ ": " $ Hit $ " at " $ HitLoc.X $ "," $ HitLoc.Y $ "," $ HitLoc.Z
				$ " normal " $ HitNorm.X $ "," $ HitNorm.Y $ "," $ HitNorm.Z);
		}
	}
}

function Run(PlayerPawn P)
{
	Say("door " $ Door.Name $ " at " $ Door.Location $ " rotation " $ Door.Rotation $ " prepivot " $ Door.PrePivot
		$ " key " $ Door.KeyNum $ " moving " $ Door.bInterpolating);
	Door.SetCollision(false, false, false);
	// the free edge: local +Y at 40, across its 8-unit thickness
	Face(P, "edge", vect(0, 1, 0), 40, vect(3, 0, 0));
	// the broad face: local +X at 4, across its 80-unit width
	Face(P, "face", vect(1, 0, 0), 4, vect(0, 30, 0));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExMover M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
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

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			Say("opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		foreach P.AllActors(class'DeusExMover', M)
			if (string(M.Name) ~= DoorWanted())
				Door = M;
		if (Door == None)
		{
			Say("no door " $ DoorWanted());
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		Say("door " $ Door.Name $ " closed at rotation " $ Door.Rotation $ ", opening");
		Door.DoOpen();
		StepTime = 0;
		Step = 1;
	}
	else if (Step == 1 && StepTime > 0.5 && !Door.bInterpolating)
	{
		StepTime = 0;
		Step = 2;
	}
	else if (Step == 2 && StepTime > 1.0)
	{
		Run(P);
		StepTime = 0;
		Step = 3;
	}
	else if (Step == 3 && StepTime > 1.0)
	{
		Say("exiting");
		P.ConsoleCommand("exit");
		Step = 4;
	}
}
