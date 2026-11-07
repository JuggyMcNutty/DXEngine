//=============================================================================
// NothingMoveConsole: moves of nothing (bugs.md PHYS.24), logged with
// "DXNOTHING:". On TargetMap ([DXCapture.NothingMoveConsole],
// 01_NYC_UNATCOIsland by default), datalinks and conversations silenced,
// 2 s in, in the console's tick:
//   1. A CapMarker, colliding with nothing, at 100, 100, 100: before each
//      move put back there, a Move, then a MoveSmooth, of each of six
//      deltas -- 0.0001 along X; 0.0001 along Z; 0.0001 along X and Y;
//      0.00008 on each axis; 0.0002 along X; 0.00009 along X -- each answer
//      and where the marker went, its offsets x 10000. The original's
//      MoveActor takes a delta whose every axis is under the double 0.0001
//      as none (Engine.dll 0x1039915a): the first four, the third and
//      fourth longer than 0.0001; the fifth is a move.
//   2. The first DeusExMover in name order that is not static and not
//      moving, with a coordinate within 1024 of 0: a Move of 0.0001 along
//      that axis, whether it moved and how far x 10000, then one back. A
//      brush's move is tested so too, before MoveActor's brush's own.
//   3. A CapMarker drawn as nothing trailing its owner (PHYS_Trailer), a
//      CapMarker whose velocity is 0.0001 along X: the trailer's rotation
//      half a second on, then with the owner's velocity 0.0002 along X
//      half a second after that. The original's physTrailer turns it
//      straight up (pitch 16384) for an owner whose velocity is nothing,
//      each axis under the double 0.0001 (0x103d7a70), and against the
//      velocity otherwise (yaw 32768).
// Then an exit.
//=============================================================================
class NothingMoveConsole extends Console;

var config string TargetMap;

var float MapTime, StepTime;
var int Step;
var string CurrentMap;
var CapMarker Owned, Trailer;

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

// A Move, then a MoveSmooth, of D, each from 100, 100, 100
function MoveOf(CapMarker M, vector D, string Label)
{
	local vector Start;
	local bool bAnswer;

	Start = vect(100,100,100);
	M.SetLocation(Start);
	bAnswer = M.Move(D);
	Log("DXNOTHING: Move " $ Label $ ": " $ bAnswer $ ", moved " $ (M.Location != Start) $ ", offsets x 10000 " $ (M.Location - Start) * 10000);
	M.SetLocation(Start);
	bAnswer = M.MoveSmooth(D);
	Log("DXNOTHING: MoveSmooth " $ Label $ ": " $ bAnswer $ ", moved " $ (M.Location != Start) $ ", offsets x 10000 " $ (M.Location - Start) * 10000);
}

function Moves(PlayerPawn P)
{
	local CapMarker M;

	M = P.Spawn(class'CapMarker',,, P.Location);
	if (M == None)
	{
		Log("DXNOTHING: the marker did not spawn");
		return;
	}
	M.SetCollision(false, false, false);
	M.bCollideWorld = false;
	MoveOf(M, vect(0.0001,0,0), "0.0001 along X");
	MoveOf(M, vect(0,0,0.0001), "0.0001 along Z");
	MoveOf(M, vect(0.0001,0.0001,0), "0.0001 along X and Y");
	MoveOf(M, vect(0.00008,0.00008,0.00008), "0.00008 on each axis");
	MoveOf(M, vect(0.0002,0,0), "0.0002 along X");
	MoveOf(M, vect(0.00009,0,0), "0.00009 along X");
	M.Destroy();
}

function MoverMove(PlayerPawn P)
{
	local DeusExMover DM, Pick;
	local vector V, Axis, Start;
	local bool bAnswer;

	foreach P.AllActors(class'DeusExMover', DM)
	{
		if (DM.bStatic || DM.bInterpolating)
			continue;
		V = DM.Location;
		if (Abs(V.X) >= 1024 && Abs(V.Y) >= 1024 && Abs(V.Z) >= 1024)
			continue;
		if (Pick == None || Caps(string(DM.Name)) < Caps(string(Pick.Name)))
			Pick = DM;
	}
	if (Pick == None)
	{
		Log("DXNOTHING: no mover near an axis's 0");
		return;
	}
	V = Pick.Location;
	if (Abs(V.X) <= Abs(V.Y) && Abs(V.X) <= Abs(V.Z))
		Axis = vect(1,0,0);
	else if (Abs(V.Y) <= Abs(V.Z))
		Axis = vect(0,1,0);
	else
		Axis = vect(0,0,1);
	Start = Pick.Location;
	bAnswer = Pick.Move(Axis * 0.0001);
	Log("DXNOTHING: mover " $ Pick.Name $ " at " $ Start $ ", Move 0.0001 along " $ Axis $ ": " $ bAnswer $ ", moved " $ (Pick.Location != Start) $ ", offsets x 10000 " $ (Pick.Location - Start) * 10000);
	Start = Pick.Location;
	bAnswer = Pick.Move(Axis * -0.0001);
	Log("DXNOTHING: mover back: " $ bAnswer $ ", moved " $ (Pick.Location != Start));
}

function Trail(PlayerPawn P)
{
	Owned = P.Spawn(class'CapMarker',,, P.Location);
	if (Owned == None)
	{
		Log("DXNOTHING: the owner did not spawn");
		return;
	}
	Trailer = P.Spawn(class'CapMarker', Owned,, P.Location);
	if (Trailer == None)
	{
		Log("DXNOTHING: the trailer did not spawn");
		return;
	}
	Owned.Velocity = vect(0.0001,0,0);
	Trailer.DrawType = DT_None;
	Trailer.SetPhysics(PHYS_Trailer);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;

	Super.Tick(Delta);
	if (Step == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	if (DXP != None)
	{
		if (DXP.dataLinkPlay != None)
			DXP.dataLinkPlay.AbortDataLink();
		if (DXP.conPlay != None)
			DXP.conPlay.TerminateConversation();
	}

	if (Step == 0)
	{
		// The original starts in its menu map.
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXNOTHING: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Step = -1;
			}
			return;
		}
		if (MapTime < 2.0)
			return;
		Moves(P);
		MoverMove(P);
		Trail(P);
		Step = 1;
		StepTime = 0;
	}
	else if (Step == -1)
	{
		if (CurrentMap ~= Target())
			Step = 0;
	}
	else if (Step == 1 && StepTime > 0.5)
	{
		if (Trailer != None)
			Log("DXNOTHING: trailer, the owner's velocity " $ Owned.Velocity $ ": physics " $ Trailer.Physics $ ", rotation " $ Trailer.Rotation);
		if (Owned != None)
			Owned.Velocity = vect(0.0002,0,0);
		Step = 2;
		StepTime = 0;
	}
	else if (Step == 2 && StepTime > 0.5)
	{
		if (Trailer != None)
			Log("DXNOTHING: trailer, the owner's velocity " $ Owned.Velocity $ ": physics " $ Trailer.Physics $ ", rotation " $ Trailer.Rotation);
		Log("DXNOTHING: done, exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
