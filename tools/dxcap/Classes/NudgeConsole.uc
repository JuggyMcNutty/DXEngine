//=============================================================================
// NudgeConsole: the script's Move, its answer and its hold-off, logged with
// "DXNUDGE:" (docs/DEVELOPMENT.md, scripted runs). From wherever the run
// starts, 2 s in: a small crate spawned 150 units ahead of the player and
// 100 up, held still (PHYS_None) and set 50 units over the floor under it;
// then each Move's answer and the crate's height over the floor after it:
// 100 units down (blocked partway by the floor); no move at all; 100 down
// from half a unit over the floor (blocked at its start); 60 up (free).
// Then the crate is destroyed and the run exits. What is compared between
// the engines: every line -- the original answers true for a move that went
// partway, and the crate stops 2 units short of the floor.
//=============================================================================
class NudgeConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXNUDGE: " $ S);
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

function float Over(Actor A, float Floor)
{
	return A.Location.Z - A.CollisionHeight - Floor;
}

function Probes(PlayerPawn P)
{
	local Actor Crate;
	local vector Spot;
	local float Floor;
	local bool b;

	Spot = P.Location + Vector(P.Rotation) * 150 + vect(0, 0, 100);
	Crate = P.Spawn(class'CrateUnbreakableSmall',,, Spot);
	if (Crate == None)
	{
		Say("no crate spawned at " $ Spot);
		return;
	}
	Crate.SetPhysics(PHYS_None);
	Floor = FloorUnder(Crate);
	Spot.Z = Floor + Crate.CollisionHeight + 50;
	if (!Crate.SetLocation(Spot))
	{
		Say("the crate would not go 50 over the floor");
		Crate.Destroy();
		return;
	}
	Say("crate " $ Crate.Name $ ", collision height " $ Crate.CollisionHeight $ ", " $ Over(Crate, Floor) $ " over the floor");

	b = Crate.Move(vect(0, 0, -100));
	Say("100 down, blocked partway: " $ b $ ", " $ Over(Crate, Floor) $ " over the floor");
	b = Crate.Move(vect(0, 0, 0));
	Say("no move: " $ b $ ", " $ Over(Crate, Floor) $ " over the floor");
	Spot.Z = Floor + Crate.CollisionHeight + 0.5;
	if (!Crate.SetLocation(Spot))
		Say("the crate would not go half a unit over the floor");
	b = Crate.Move(vect(0, 0, -100));
	Say("100 down from half a unit over the floor, blocked at the start: " $ b $ ", " $ Over(Crate, Floor) $ " over the floor");
	b = Crate.Move(vect(0, 0, 60));
	Say("60 up, free: " $ b $ ", " $ Over(Crate, Floor) $ " over the floor");
	Crate.Destroy();
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
