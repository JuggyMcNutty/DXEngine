//=============================================================================
// PathConsole: an actor following a path of InterpolationPoints, logged
// with "DXPATH:" (docs/DEVELOPMENT.md, scripted runs). From wherever the
// run starts, 2 s in: four InterpolationPoints spawned over the player,
// 100 units up and 100 along +X, then 120 further along each, the third
// 60 aside and 40 higher, each turned a bit more (yaw an eighth of a turn
// on, pitch and roll a little), linked in a ring (the last's Next the
// first, as their BeginPlay links a path), the last the end of the path;
// a CapRider set on it at the first point, at a rate of 1 a second. Nine
// frames are held half a second each, so the rider's ticks are 0.4 s and
// its alpha steps 0.4: each tick's place and rotation, the spline through
// the four points, and each point's InterpolateEnd, where the time past the
// point goes on to the next. The ride ends at the fourth point. What is
// compared between the engines: every line.
//=============================================================================
class PathConsole extends Console;

var float RunTime;
var int Phase, Holds;
var CapRider Rider;
var InterpolationPoint Points[4];

function Say(string S)
{
	Log("DXPATH: " $ S);
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
}

function bool MakePath(PlayerPawn P)
{
	local vector Base, Spots[4];
	local rotator Turns[4];
	local int i;

	Base = P.Location + vect(100, 0, 100);
	Spots[0] = Base;
	Spots[1] = Base + vect(120, 0, 0);
	Spots[2] = Base + vect(240, 60, 40);
	Spots[3] = Base + vect(360, 0, 0);
	Turns[0] = rot(0, 0, 0);
	Turns[1] = rot(2048, 8192, 0);
	Turns[2] = rot(-2048, 16384, 1024);
	Turns[3] = rot(0, 24576, 0);
	for (i = 0; i < 4; i++)
	{
		Points[i] = P.Spawn(class'InterpolationPoint',, 'CapPath', Spots[i], Turns[i]);
		if (Points[i] == None)
		{
			Say("no point " $ i $ " at " $ Spots[i]);
			return False;
		}
		Points[i].Position = i;
	}
	for (i = 0; i < 4; i++)
	{
		Points[i].Prev = Points[(i + 3) % 4];
		Points[i].Next = Points[(i + 1) % 4];
	}
	Points[3].bEndOfPath = True;
	Say("four points: " $ Points[0].Location $ " " $ Points[0].Rotation $ "; " $ Points[1].Location $ " " $ Points[1].Rotation $ "; " $ Points[2].Location $ " " $ Points[2].Rotation $ "; " $ Points[3].Location $ " " $ Points[3].Rotation);
	Rider = P.Spawn(class'CapRider',,, Points[0].Location, Points[0].Rotation);
	if (Rider == None)
	{
		Say("no rider");
		return False;
	}
	Rider.Target = Points[0];
	Rider.PhysRate = 1.0;
	Rider.PhysAlpha = 0.0;
	Rider.bInterpolating = True;
	Rider.SetPhysics(PHYS_Interpolating);
	Rider.TicksLeft = 11;
	Say("rider " $ Rider.Name $ " at " $ Rider.Location $ ", rate " $ Rider.PhysRate);
	return True;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Hold(P, 0.5);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		if (!MakePath(P))
		{
			Say("exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Hold(P, 0.5);
		Holds = 1;
		Phase = 2;
	}
	else if (Phase == 2)
	{
		Hold(P, 0.5);
		Holds++;
		if (Holds >= 9)
		{
			Say("frames held: " $ Holds);
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		Say("the rider now at " $ Rider.Location $ ", rotation " $ Rider.Rotation $ ", " $ GetEnum(enum'EPhysics', Rider.Physics) $ "; done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
