//=============================================================================
// ShotConsole: a bouncing projectile's frame, logged with "DXSHOT:"
// (docs/DEVELOPMENT.md, scripted runs). From wherever the run starts, 2 s
// in: a CapShot spawned 150 units ahead of the player and 100 up, then set
// 50 units over the floor under it, with that frame and the next each held
// half a second by the console's own line traces (as FrameConsole holds
// one), so that the actors' next tick is the longest allowed, 0.4 s; in the
// second held frame the shot is set flying as a projectile at 400 units a
// second along +X and 400 down. In that one tick it meets the floor, HitWall
// turns it up at 0.6 of its speed, and the original's physProjectile gives
// the step's time not moved back and moves again by the velocity x the
// whole tick, not the rest: the shot ends the tick 129 units along and 63
// up from where it started. The shot logs the hit, its next ticks, its
// place and velocity; the run exits 2 s on. What is compared between the
// engines: every line.
//=============================================================================
class ShotConsole extends Console;

var float RunTime, ArmedAt;
var int Phase;
var CapShot Shot;

function Say(string S)
{
	Log("DXSHOT: " $ S);
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
		Shot = P.Spawn(class'CapShot',,, Spot);
		if (Shot == None)
		{
			Say("no shot spawned at " $ Spot $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Floor = FloorUnder(Shot);
		Spot.Z = Floor + Shot.CollisionHeight + 50;
		if (!Shot.SetLocation(Spot))
			Say("the shot would not go 50 over the floor");
		Say("shot " $ Shot.Name $ " at " $ Shot.Location $ ", the floor at " $ Floor);
		Hold(P, 0.5);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		Shot.SetPhysics(PHYS_Projectile);
		Shot.Velocity = vect(400, 0, -400);
		Shot.TicksLeft = 3;
		Say("set flying at " $ Shot.Velocity);
		Hold(P, 0.5);
		ArmedAt = RunTime;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime - ArmedAt > 2.0)
	{
		if (Shot != None)
			Say("the shot now at " $ Shot.Location $ ", velocity " $ Shot.Velocity $ ", physics " $ GetEnum(enum'EPhysics', Shot.Physics) $ "; done, exiting");
		else
			Say("the shot gone; done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
