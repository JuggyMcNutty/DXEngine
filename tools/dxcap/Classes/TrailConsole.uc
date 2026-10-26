//=============================================================================
// TrailConsole: the trailer physics, logged with "DXTRAIL:"
// (docs/DEVELOPMENT.md, scripted runs). From wherever the run starts, 2 s
// in: a small crate spawned 100 units over the player, held still, and four
// trailers owned by it: a sprite with bTrailerSameRotation and a Mass of 30
// (it rides 30 units behind the crate's facing), a sprite with
// bTrailerPrePivot and a PrePivot of 50 up, a plain trailer with
// bTrailerSameRotation (it takes the crate's rotation), and a plain one
// without (it faces against the crate's velocity, or straight up with
// none). Frames are held half a second each; between them the crate is
// turned a quarter turn, moved 50 along +X and given a velocity of 100
// along +Y, then no velocity. Each tick, every trailer's place and
// rotation. What is compared between the engines: every line.
//=============================================================================
class TrailConsole extends Console;

var float RunTime;
var int Phase;
var Actor Crate;
var CapTrail Trails[4];

function Say(string S)
{
	Log("DXTRAIL: " $ S);
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

function bool MakeTrails(PlayerPawn P)
{
	local int i;

	Crate = P.Spawn(class'CrateUnbreakableSmall',,, P.Location + vect(0, 0, 100));
	if (Crate == None)
	{
		Say("no crate");
		return False;
	}
	Crate.SetPhysics(PHYS_None);
	Trails[0] = P.Spawn(class'SpriteTrail', Crate,, Crate.Location);
	Trails[1] = P.Spawn(class'SpriteTrail', Crate,, Crate.Location);
	Trails[2] = P.Spawn(class'CapTrail', Crate,, Crate.Location);
	Trails[3] = P.Spawn(class'CapTrail', Crate,, Crate.Location);
	for (i = 0; i < 4; i++)
		if (Trails[i] == None)
		{
			Say("no trailer " $ i);
			return False;
		}
	Trails[0].bTrailerSameRotation = True;
	Trails[0].Mass = 30;
	Trails[1].bTrailerPrePivot = True;
	Trails[1].PrePivot = vect(0, 0, 50);
	Trails[2].bTrailerSameRotation = True;
	Say("crate " $ Crate.Name $ " at " $ Crate.Location $ " " $ Crate.Rotation $ ", trailers " $ Trails[0].Name $ " " $ Trails[1].Name $ " " $ Trails[2].Name $ " " $ Trails[3].Name);
	return True;
}

function Report(string When)
{
	local int i;
	local string S;

	S = When $ ":";
	for (i = 0; i < 4; i++)
		S = S $ " " $ (Trails[i].Location - Crate.Location) $ " " $ Trails[i].Rotation $ " |";
	Say(S);
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
		if (!MakeTrails(P))
		{
			Say("exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Hold(P, 0.5);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		Report("at rest, facing " $ Crate.Rotation);
		Crate.SetRotation(rot(0, 16384, 0));
		Crate.Velocity = vect(0, 100, 0);
		Hold(P, 0.5);
		Phase = 2;
	}
	else if (Phase == 2)
	{
		Report("turned a quarter, moving along +Y");
		Crate.SetLocation(Crate.Location + vect(50, 0, 0));
		Crate.Velocity = vect(0, 0, 0);
		Hold(P, 0.5);
		Phase = 3;
	}
	else if (Phase == 3)
	{
		Report("moved 50 along +X, still");
		Hold(P, 0.5);
		Phase = 4;
	}
	else if (Phase == 4)
	{
		Report("a tick on");
		Say("done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
