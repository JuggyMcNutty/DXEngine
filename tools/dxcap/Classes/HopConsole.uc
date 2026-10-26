//=============================================================================
// HopConsole: a fall's step split where its Z velocity turns over, logged
// with "DXHOP:" (docs/DEVELOPMENT.md, scripted runs). From wherever the run
// starts, 2 s in: a CapHopper spawned 150 units ahead of the player and 100
// up, then set 1 unit over the floor under it, with that frame and the next
// each held half a second by the console's own line traces (as
// FrameConsole holds one), so that the actors' next tick is the longest
// allowed, 0.4 s, stepped 0.1 s at a time; in the second held frame the
// actor is set falling, rising at 30 units a second. Its first step turns
// over: the original splits it at the turn, so the actor's next step, a
// whole one, meets the floor at -77.5 units a second, where one unsplit
// step meets it at the step's mean, -17.5. The actor logs its ticks and its
// landing's velocity; the run exits 2 s on. What is compared between the
// engines: every line.
//=============================================================================
class HopConsole extends Console;

var float RunTime, ArmedAt;
var int Phase;
var CapHopper Hopper;

function Say(string S)
{
	Log("DXHOP: " $ S);
}

// The floor under the actor's centre, by a line straight down.
function float FloorUnder(Actor A)
{
	local vector HitLoc, HitNorm;
	local Actor Hit;

	Hit = A.Trace(HitLoc, HitNorm, A.Location - vect(0, 0, 2000), A.Location, False);
	if (Hit == None)
		return A.Location.Z - 2000;
	return HitLoc.Z;
}

// The frame held Seconds by line traces timed in short spans.
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
		Spot = P.Location + Vector(P.Rotation) * 150 + vect(0, 0, 100);
		Hopper = P.Spawn(class'CapHopper',,, Spot);
		if (Hopper == None)
		{
			Say("no hopper spawned at " $ Spot $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Floor = FloorUnder(Hopper);
		Spot.Z = Floor + Hopper.CollisionHeight + 1;
		if (!Hopper.SetLocation(Spot))
			Say("the hopper would not go 1 unit over the floor");
		Say("hopper " $ Hopper.Name $ " at " $ Hopper.Location $ ", the floor at " $ Floor $ ", gravity " $ Hopper.Region.Zone.ZoneGravity);
		Hold(P, 0.5);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		Hopper.SetPhysics(PHYS_Falling);
		Hopper.Velocity = vect(0, 0, 30);
		Hopper.TicksLeft = 3;
		Say("set falling, rising at " $ Hopper.Velocity.Z);
		Hold(P, 0.5);
		ArmedAt = RunTime;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime - ArmedAt > 2.0)
	{
		Say("physics now " $ GetEnum(enum'EPhysics', Hopper.Physics) $ ", at " $ Hopper.Location $ "; done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
