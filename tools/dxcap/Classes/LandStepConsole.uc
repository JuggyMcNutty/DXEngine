//=============================================================================
// LandStepConsole (bugs.md PHYS.32): the walk after a landing within a
// long tick -- from wherever the run starts (Liberty Island's start, the
// dock's flat ground, as HopConsole's), 2 s in: a CapReacher spawned 150
// units ahead of the player, set 1 unit over the floor under it, with that
// frame held half a second by the console's own traces (HopConsole's Hold),
// so that the actors' next tick is the longest, 0.4 s; in the next frame,
// held again, it is set falling with a velocity of 300 along the player's
// facing, level, and an acceleration of its AccelRate the same way, a
// DesiredSpeed of 1. It lands within the fall's first 0.1 s step, its
// remaining time over 0.35 s, and walks the rest of the tick in steps of at
// most 0.05 s. Logged: its place and velocity at the console's next three
// ticks, and how far it went along the facing.
//
// The original's processLanded hands the fall's step count to physWalking
// (Engine.dll 0x103d20ce: Iterations pushed; 0x103cef60 calls physWalking
// with it), whose loop stops at 8 counting it, so the walk takes 7 steps,
// 0.35 s, and leaves the rest of the tick unwalked; DXEngine's walk starts
// its count at 0 (DeusExPhysLanded -> TickWalking), takes 8 and walks it
// all: some 0.03 s x 300 = 9 units further along in DXEngine. Each line
// starts "DXLANDSTEP:".
//=============================================================================
class LandStepConsole extends Console;

var float RunTime, ArmedAt;
var int Phase, After;
var CapReacher Walker;
var vector Start, Dir;

function Say(string S)
{
	Log("DXLANDSTEP: " $ S);
}

function float FloorUnder(Actor A)
{
	local vector HitLoc, HitNorm;
	local Actor Hit;

	Hit = A.Trace(HitLoc, HitNorm, A.Location - vect(0, 0, 2000), A.Location, False);
	if (Hit == None)
		return A.Location.Z - 2000;
	return HitLoc.Z;
}

function Hold(PlayerPawn P, float Seconds)
{
	local vector HitLocation, HitNormal;
	local float Stalled;
	local int c, i, Spans;

	while (Stalled < Seconds && Spans < 4000)
	{
		c = 0;
		clock(c);
		for (i = 0; i < 1000; i++)
		{
			P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
			P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
			P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
			P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
		}
		unclock(c);
		Stalled += CyclesToSeconds(c);
		Spans++;
	}
	Say("a frame held " $ Seconds $ " s: " $ (Stalled >= Seconds));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector Spot;
	local float Floor;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Dir = vector(P.Rotation);
		Dir.Z = 0;
		Dir = Normal(Dir);
		Spot = P.Location + Dir * 150;
		Walker = P.Spawn(class'CapReacher',,, Spot);
		if (Walker == None)
		{
			Say("no walker spawned at " $ Spot $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Floor = FloorUnder(Walker);
		Spot.Z = Floor + Walker.CollisionHeight + 1;
		if (!Walker.SetLocation(Spot))
			Say("the walker would not go 1 unit over the floor");
		Walker.DesiredSpeed = 1.0;
		Say("walker at " $ Walker.Location $ ", the floor at " $ Floor $ ", facing " $ Dir);
		Hold(P, 0.5);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		Start = Walker.Location;
		Walker.SetPhysics(PHYS_Falling);
		Walker.Velocity = Dir * 300;
		Walker.Acceleration = Dir * Walker.AccelRate;
		Say("set falling at " $ Walker.Velocity $ ", acceleration " $ Walker.Acceleration);
		Hold(P, 0.5);
		Phase = 2;
	}
	else if (Phase == 2)
	{
		After++;
		Say("tick " $ After $ " at " $ Walker.Location $ " along " $ ((Walker.Location - Start) dot Dir)
			$ " velocity " $ Walker.Velocity $ " physics " $ GetEnum(enum'EPhysics', Walker.Physics));
		if (After >= 3)
		{
			Say("done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
