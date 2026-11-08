//=============================================================================
// PickWallConsole: PickWallAdjust of a pawn facing walls of several kinds
// (bugs.md PHYS.27), logged with "DXPICKWALL:". On TargetMap
// ([DXCapture.PickWallConsole], 01_NYC_UNATCOIsland by default), opened from
// wherever the run starts, datalinks and conversations silenced, 2 s into
// the level's time: along 32 yaws from the player, 10 units over its feet,
// within Reach (600), the faces met (a normal within 0.3 of level) sorted by
// the height of their top over the player's feet, a line down from 200 over
// a point 8 units inside: Low, at most MaxStepHeight (25), a step; Ledge, up
// to 48, a jump; Tall, over 96 or no top. PickWallAdjust traces the world
// and movers alone, so only the level's own faces count. For the first of
// each kind found, one case after another, a CapReacher (DesiredSpeed 1)
// 40 units out from the face, its feet at the player's, let stand 0.5 s;
// then its Destination 300 units through the face, its acceleration toward
// it, and PickWallAdjust called: its answer, the Destination, velocity,
// acceleration, physics and bJumpOffPawn after it, and 0.5 s later its place
// and physics; then it goes. The cases: each kind walking with bCanJump off
// and on; the Tall face flying, the Destination 100 over it and 100 under
// it; walking with the Destination straight over it (no level way, false)
// and 10 units off (within its radius, false). Then an exit. The reacher is
// no player, so the original's first jump test (a player's alone) and its
// random turn off the wall never come.
//=============================================================================
class PickWallConsole extends Console;

var config string TargetMap;
var config float Reach;

var float MapTime, CaseTime;
var int Step, CaseNum, Phase;
var string CurrentMap;
var vector Faces[3], Normals[3];
var float Heights[3];
var int bFound[3];  // 1 when found (UCC takes no bool arrays)
var string Kinds[3];
var CapReacher Mover;
var vector FeetSpot;

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

// The faces round the player, the first of each kind.
function FindFaces(PlayerPawn P)
{
	local int d, k;
	local rotator R;
	local vector Start, HitLoc, HitNorm, Inside, TopLoc, TopNorm;
	local float Range, Feet, Height;
	local Actor A;

	Kinds[0] = "Low";
	Kinds[1] = "Ledge";
	Kinds[2] = "Tall";
	Range = Reach;
	if (Range <= 0)
		Range = 600;
	Feet = P.Location.Z - P.CollisionHeight;
	Start = P.Location;
	Start.Z = Feet + 10;
	for (d = 0; d < 32; d++)
	{
		R.Yaw = d * 2048;
		A = P.Trace(HitLoc, HitNorm, Start + Range * vector(R), Start, false);
		if (A == None || Abs(HitNorm.Z) >= 0.3)
			continue;
		Inside = HitLoc - HitNorm * 8;
		A = P.Trace(TopLoc, TopNorm, Inside, Inside + vect(0,0,200), false);
		if (A == None)
			Height = 200;
		else
			Height = TopLoc.Z - Feet;
		if (Height <= 25)
			k = 0;
		else if (Height <= 48)
			k = 1;
		else if (Height > 96)
			k = 2;
		else
			k = -1;
		Log("DXPICKWALL: yaw " $ R.Yaw $ " face at " $ HitLoc $ ", normal " $ HitNorm $ ", top " $ Height $ " over the feet");
		if (k >= 0 && bFound[k] == 0)
		{
			bFound[k] = 1;
			Faces[k] = HitLoc;
			Normals[k] = HitNorm;
			Heights[k] = Height;
		}
	}
	for (k = 0; k < 3; k++)
	{
		if (bFound[k] != 0)
			Log("DXPICKWALL: " $ Kinds[k] $ " face at " $ Faces[k] $ ", normal " $ Normals[k] $ ", top " $ Heights[k]);
		else
			Log("DXPICKWALL: no " $ Kinds[k] $ " face");
	}
	FeetSpot = P.Location;
	FeetSpot.Z = Feet;
}

// Case n's face: Low, Ledge or Tall; -1 for none found.
function int CaseFace(int n)
{
	local int k;

	if (n < 2)
		k = 0;
	else if (n < 4)
		k = 1;
	else
		k = 2;
	if (bFound[k] == 0)
		return -1;
	return k;
}

function string CaseName(int n)
{
	switch (n)
	{
	case 0: return "Low-walk";
	case 1: return "Low-jump";
	case 2: return "Ledge-walk";
	case 3: return "Ledge-jump";
	case 4: return "Tall-walk";
	case 5: return "Tall-jump";
	case 6: return "Tall-fly-over";
	case 7: return "Tall-fly-under";
	case 8: return "Tall-above";
	case 9: return "Tall-near";
	}
	return "";
}

function bool StartCase(PlayerPawn P, int n)
{
	local int k;
	local vector Spot;

	k = CaseFace(n);
	if (k < 0)
	{
		Log("DXPICKWALL: " $ CaseName(n) $ " skipped, no face");
		return false;
	}
	Spot = Faces[k] + Normals[k] * 40;
	Spot.Z = FeetSpot.Z + class'CapReacher'.default.CollisionHeight + 1;
	Mover = P.Spawn(class'CapReacher',,, Spot);
	if (Mover == None)
	{
		Log("DXPICKWALL: " $ CaseName(n) $ " no room at " $ Spot);
		return false;
	}
	Mover.DesiredSpeed = 1.0;
	Mover.bCanJump = (n == 1 || n == 3 || n == 5);
	if (n == 6 || n == 7)
	{
		Mover.bCanFly = true;
		Mover.SetPhysics(PHYS_Flying);
		Mover.SetLocation(Spot + vect(0,0,30));
	}
	else
		Mover.SetPhysics(PHYS_Walking);
	return true;
}

function Ask(int n)
{
	local int k;
	local bool bResult;

	k = CaseFace(n);
	Mover.Destination = Mover.Location - Normals[k] * 300;
	if (n == 6)
		Mover.Destination.Z += 100;
	else if (n == 7)
		Mover.Destination.Z -= 100;
	else if (n == 8)
		Mover.Destination = Mover.Location + vect(0,0,300);
	else if (n == 9)
		Mover.Destination = Mover.Location - Normals[k] * 10;
	Mover.Acceleration = Normal(Mover.Destination - Mover.Location) * Mover.AccelRate;
	Log("DXPICKWALL: " $ CaseName(n) $ " at " $ Mover.Location $ ", physics " $ int(Mover.Physics) $ ", destination " $ Mover.Destination);
	bResult = Mover.PickWallAdjust();
	Log("DXPICKWALL: " $ CaseName(n) $ " PickWallAdjust " $ bResult $ ", destination " $ Mover.Destination
		$ ", velocity " $ Mover.Velocity $ ", acceleration " $ Mover.Acceleration $ ", physics " $ int(Mover.Physics)
		$ ", jump off pawn " $ Mover.bJumpOffPawn);
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
	P.ReducedDamageType = 'All';
	if (DXP != None)
	{
		if (DXP.dataLinkPlay != None)
			DXP.dataLinkPlay.AbortDataLink();
		if (DXP.conPlay != None)
			DXP.conPlay.TerminateConversation();
	}

	if (Step == 0)
	{
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXPICKWALL: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Step = -1;
			}
			return;
		}
		if (P.Level.TimeSeconds < 2.0)
			return;
		FindFaces(P);
		CaseNum = 0;
		Phase = 0;
		Step = 1;
	}
	else if (Step == -1)
	{
		if (CurrentMap ~= Target())
			Step = 0;
	}
	else if (Step == 1)
	{
		if (CaseNum > 9)
		{
			Log("DXPICKWALL: done, exiting");
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		CaseTime += Delta;
		if (Phase == 0)
		{
			CaseTime = 0;
			if (StartCase(P, CaseNum))
				Phase = 1;
			else
				CaseNum++;
		}
		else if (Phase == 1 && CaseTime >= 0.5)
		{
			Ask(CaseNum);
			CaseTime = 0;
			Phase = 2;
		}
		else if (Phase == 2 && CaseTime >= 0.5)
		{
			Log("DXPICKWALL: " $ CaseName(CaseNum) $ " 0.5 s on at " $ Mover.Location $ ", velocity " $ Mover.Velocity
				$ ", physics " $ int(Mover.Physics));
			Mover.Destroy();
			Mover = None;
			Phase = 0;
			CaseNum++;
		}
	}
}
