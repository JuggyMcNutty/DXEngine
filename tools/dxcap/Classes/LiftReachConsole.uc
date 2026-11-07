//=============================================================================
// LiftReachConsole: the reach tests' moves where they meet a mover. The
// original's walk and fly moves (walkMove 0x103c3290, flyMove 0x103c3900)
// are ULevel::MoveActor test moves with bIgnoreBases 0, so what the pawn
// stands on stops them as anything else does, and what blocks is decided
// by IsBlockedBy (the filter at 0x103995f1): a mover blocks a pawn that
// collides with the world by the mover's bBlockActors, whatever the pawn's
// own. DXEngine passed through the pawn's base, and through a mover unless
// both blocked actors (bugs.md PHYS.20).
//
// On TargetMap ([DXCapture.LiftReachConsole], 00_TrainingFinal by default),
// conversations stopped, the player out of the world from its first tick
// there, 2 s in:
//   1. the lift: the first ElevatorMover by name, else the first other
//      mover by name that is no door, at least 96 units across each way,
//      whose floor a line down its box's middle meets -- from the middle
//      (a car's floor) or from over its top (a platform's). A CapReacher
//      dropped onto that floor; once it stands there (its Base the lift and
//      walking, or 3 s on), its place and base, then for yaws 0, 16384,
//      32768 and 49152 AIDirectionReachable from it (10 to 200 units) and
//      PointReachable of a spot 40 units along each.
//   2. the door: the first DeusExMover door by name that collides and
//      blocks actors and is thinnest across X or Y; a line across its box's
//      middle along that side, 32 units past each face (MoverConsole's). At
//      the near end in turn a CapReacher walker landed on the floor, a
//      CapReacher flyer 16 units round, each with bBlockActors off and then
//      on, and a stock Seagull with its own flags, flying: each asks
//      AIDirectionReachable along the line toward the far end, the range
//      within 24 units of it, so only a pawn that gets through the door
//      finds it.
// Each answer and spot logged with "DXLIFT:"; then an exit. What is
// compared between the engines: every line. In the original the lift's
// floor holds the walker, and the door stops every walker and flyer.
//=============================================================================
class LiftReachConsole extends Console;

var config string TargetMap;

var float MapTime, StepTime;
var int Step;
var string CurrentMap;
var Object Stopped;
var bool bOut;
var Mover Lift;
var vector LiftFloor;
var DeusExMover Door;
var vector DoorS, DoorT, DoorMin;
var CapReacher Rider, Walker, Flyer;

function string Target()
{
	if (TargetMap == "")
		return "00_TrainingFinal";
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

function string V(vector X)
{
	return int(X.X) $ "," $ int(X.Y) $ "," $ int(X.Z);
}

function Say(string S)
{
	Log("DXLIFT: " $ S);
}

// Where a line down the mover's box meets its floor: from the box's middle
// (a car), else from over its top (a platform); the mover must be what it
// meets, facing up.
function bool FloorOf(PlayerPawn P, Mover M, out vector Spot, out string How)
{
	local vector Min, Max, C, From, To, HitLoc, HitNorm;
	local Actor Hit;

	if (!M.GetBoundingBox(Min, Max))
		return false;
	if (Max.X - Min.X < 96 || Max.Y - Min.Y < 96)
		return false;
	C = (Min + Max) * 0.5;
	To = C;
	To.Z = Min.Z - 8;
	Hit = P.Trace(HitLoc, HitNorm, To, C, false);
	if (Hit == M && HitNorm.Z > 0.7)
	{
		Spot = HitLoc;
		How = "inside";
		return true;
	}
	From = C;
	From.Z = Max.Z + 96;
	Hit = P.Trace(HitLoc, HitNorm, To, From, false);
	if (Hit == M && HitNorm.Z > 0.7)
	{
		Spot = HitLoc;
		How = "on top";
		return true;
	}
	return false;
}

function ChooseLift(PlayerPawn P)
{
	local Mover M, Best;
	local vector Spot, BestSpot;
	local string How, BestHow;
	local bool bElevator, bBestElevator;

	foreach P.AllActors(class'Mover', M)
	{
		bElevator = M.IsA('ElevatorMover');
		if (!bElevator && DeusExMover(M) != None && DeusExMover(M).bIsDoor)
			continue;
		if (Best != None)
		{
			if (bBestElevator && !bElevator)
				continue;
			if (bElevator == bBestElevator && Caps(string(M.Name)) > Caps(string(Best.Name)))
				continue;
		}
		if (!FloorOf(P, M, Spot, How))
			continue;
		Best = M;
		BestSpot = Spot;
		BestHow = How;
		bBestElevator = bElevator;
	}
	Lift = Best;
	LiftFloor = BestSpot;
	if (Lift == None)
	{
		Say("lift: none found");
		return;
	}
	Say("lift " $ Lift.Name $ " (" $ Lift.Class $ ") at " $ V(Lift.Location) $ " floor " $ BestHow $ " at " $ V(LiftFloor)
		$ " collides " $ Lift.bCollideActors $ " blocks " $ Lift.bBlockActors);
	Rider = P.Spawn(class'CapReacher',,, LiftFloor + vect(0, 0, 44));
	if (Rider == None)
	{
		Say("lift: the rider did not spawn");
		return;
	}
	Rider.SetPhysics(PHYS_Falling);
}

// The line across a mover's box, through its middle along its thinnest
// side, 32 units past each face; a door thinnest along Z is no use here
function bool Across(Mover M, out vector S, out vector T, out vector Min)
{
	local vector Max, C, E, A;

	if (!M.GetBoundingBox(Min, Max))
		return false;
	C = (Min + Max) * 0.5;
	E = (Max - Min) * 0.5;
	if (E.X <= E.Y && E.X <= E.Z)
		A = vect(1,0,0) * (E.X + 32);
	else if (E.Y <= E.Z)
		A = vect(0,1,0) * (E.Y + 32);
	else
		return false;
	S = C - A;
	T = C + A;
	return true;
}

function ChooseDoor(PlayerPawn P)
{
	local DeusExMover M;
	local vector S, T, Min, Spot;

	foreach P.AllActors(class'DeusExMover', M)
	{
		if (M.IsA('BreakableGlass') || !M.bIsDoor || !M.bCollideActors || !M.bBlockActors)
			continue;
		if (Door != None && Caps(string(M.Name)) > Caps(string(Door.Name)))
			continue;
		if (!Across(M, S, T, Min))
			continue;
		Door = M;
		DoorS = S;
		DoorT = T;
		DoorMin = Min;
	}
	if (Door == None)
	{
		Say("door: none found");
		return;
	}
	Say("door " $ Door.Name $ " at " $ V(Door.Location) $ " line " $ V(DoorS) $ " to " $ V(DoorT));
	Spot = DoorS;
	Spot.Z = DoorMin.Z + 44;
	Walker = P.Spawn(class'CapReacher',,, Spot);
	if (Walker != None)
		Walker.SetPhysics(PHYS_Falling);
	else
		Say("door: the walker did not spawn");
}

function ProbeLift()
{
	local int i, yaw;
	local vector Dest, Spot;
	local rotator R;
	local bool bFound;

	if (Rider == None)
		return;
	Say("lift rider at " $ V(Rider.Location) $ " base " $ Rider.Base $ " physics " $ Rider.Physics);
	for (i = 0; i < 4; i++)
	{
		yaw = i * 16384;
		bFound = Rider.AIDirectionReachable(Rider.Location, yaw, 0, 10, 200, Dest);
		Say("lift direction " $ yaw $ " found " $ bFound $ " at " $ V(Dest) $ " dist " $ VSize(Dest - Rider.Location));
		R.Yaw = yaw;
		Spot = Rider.Location + vector(R) * 40;
		Say("lift point " $ yaw $ " reachable " $ Rider.PointReachable(Spot));
	}
}

function AskDoor(Pawn X, string Who)
{
	local vector Goal, Dest;
	local rotator R;
	local bool bFound;

	Goal = DoorT;
	Goal.Z = X.Location.Z;
	R = rotator(Goal - X.Location);
	bFound = X.AIDirectionReachable(Goal, R.Yaw, 0, 0, 24, Dest);
	Say("door " $ Who $ " at " $ V(X.Location) $ " physics " $ X.Physics $ " collides world " $ X.bCollideWorld
		$ " blocks " $ X.bBlockActors $ ": found " $ bFound $ " at " $ V(Dest));
}

function ProbeDoor(PlayerPawn P)
{
	local Seagull Gull;

	if (Door == None)
		return;
	if (Walker != None)
	{
		Walker.SetCollision(true, false, false);
		AskDoor(Walker, "walker");
		Walker.SetCollision(true, true, true);
		AskDoor(Walker, "walker");
		Walker.Destroy();
		Walker = None;
	}
	// Each flyer where the walker stood, once it is gone
	Flyer = P.Spawn(class'CapReacher',,, DoorS);
	if (Flyer != None)
	{
		Flyer.SetCollisionSize(16, 16);
		Flyer.bCanFly = True;
		Flyer.SetPhysics(PHYS_Flying);
		Flyer.SetCollision(true, false, false);
		AskDoor(Flyer, "flyer");
		Flyer.SetCollision(true, true, true);
		AskDoor(Flyer, "flyer");
		Flyer.Destroy();
		Flyer = None;
	}
	else
		Say("door: the flyer did not spawn");
	Gull = P.Spawn(class'Seagull',,, DoorS);
	if (Gull != None)
	{
		Gull.SetPhysics(PHYS_Flying);
		AskDoor(Gull, "seagull");
		Gull.Destroy();
	}
	else
		Say("door: the seagull did not spawn");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;

	Super.Tick(Delta);
	if (Step == 3 || Viewport == None || Viewport.Actor == None)
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
	if (DXP != None && DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		DXP.conPlay.TerminateConversation();
	}

	if (Step == 0)
	{
		// The original starts in its menu map.
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Say("opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Step = -1;
			}
			return;
		}
		if (!bOut)
		{
			P.ReducedDamageType = 'All';
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (MapTime < 2.0 || DXP == None)
			return;
		ChooseLift(P);
		ChooseDoor(P);
		Step = 1;
		StepTime = 0;
	}
	else if (Step == -1)
	{
		if (CurrentMap ~= Target())
			Step = 0;
	}
	else if (Step == 1)
	{
		if (StepTime < 3.0)
		{
			if (Rider != None && (Rider.Base != Lift || Rider.Physics != PHYS_Walking))
				return;
			if (Walker != None && Walker.Physics != PHYS_Walking)
				return;
		}
		ProbeLift();
		ProbeDoor(P);
		if (Rider != None)
			Rider.Destroy();
		if (Walker != None)
			Walker.Destroy();
		if (Flyer != None)
			Flyer.Destroy();
		Say("done, exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
