//=============================================================================
// WalkConsole: the actor iterators as the level changes under them, logged
// with "DXWALK:" (docs/DEVELOPMENT.md, scripted runs). From wherever the
// run starts, 2 s in: a CapLoopMark spawned 100 units over the player,
// then four foreach loops, each over the CapLoopMarks of the level (the
// run's own, so each starts with one): RadiusActors within 300 units of the
// player, VisibleActors within 300, ZoneActors of the player's zone, and
// ChildActors of the player (the markers owned by it) -- each body spawns
// another marker (owned by the player, in reach) on its first pass, and
// logs each actor the loop gives; then BasedActors of a marker the player
// is based on, its body spawning a second based marker; then an exit. What
// is compared between the engines: every line -- the original's walks of
// the level's list reach the marker spawned during the loop.
//=============================================================================
class WalkConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXWALK: " $ S);
}

function CapLoopMark Make(PlayerPawn P, vector At)
{
	local CapLoopMark A;

	A = P.Spawn(class'CapLoopMark', P,, At);
	if (A != None)
		A.SetPhysics(PHYS_None);
	return A;
}

function Probes(PlayerPawn P)
{
	local CapLoopMark A, First, Made;
	local vector Over;
	local int Seen;

	Over = P.Location + vect(0, 0, 100);
	First = Make(P, Over);
	if (First == None)
	{
		Say("no marker spawned");
		return;
	}
	Say("one marker, " $ First.Name $ ", owner " $ First.Owner);

	Seen = 0;
	foreach P.RadiusActors(class'CapLoopMark', A, 300)
	{
		Seen++;
		if (Seen == 1)
			Made = Make(P, Over + vect(20, 0, 0));
		Say("RadiusActors " $ Seen $ ": " $ A.Name);
	}
	Say("RadiusActors gave " $ Seen);

	Seen = 0;
	foreach P.VisibleActors(class'CapLoopMark', A, 300)
	{
		Seen++;
		if (Seen == 1)
			Make(P, Over + vect(40, 0, 0));
		Say("VisibleActors " $ Seen $ ": " $ A.Name);
	}
	Say("VisibleActors gave " $ Seen);

	Seen = 0;
	foreach P.Region.Zone.ZoneActors(class'CapLoopMark', A)
	{
		Seen++;
		if (Seen == 1)
			Make(P, Over + vect(60, 0, 0));
		Say("ZoneActors " $ Seen $ ": " $ A.Name);
	}
	Say("ZoneActors gave " $ Seen);

	Seen = 0;
	foreach P.ChildActors(class'CapLoopMark', A)
	{
		Seen++;
		if (Seen == 1)
			Make(P, Over + vect(80, 0, 0));
		Say("ChildActors " $ Seen $ ": " $ A.Name);
	}
	Say("ChildActors gave " $ Seen);

	// Two markers based on the first: the second made during the loop
	A = Make(P, Over + vect(100, 0, 0));
	if (A != None)
		A.SetBase(First);
	Seen = 0;
	foreach First.BasedActors(class'CapLoopMark', A)
	{
		Seen++;
		if (Seen == 1)
		{
			Made = Make(P, Over + vect(120, 0, 0));
			if (Made != None)
				Made.SetBase(First);
		}
		Say("BasedActors " $ Seen $ ": " $ A.Name);
	}
	Say("BasedActors gave " $ Seen);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes(Viewport.Actor);
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
