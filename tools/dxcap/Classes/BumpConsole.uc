//=============================================================================
// BumpConsole: the BumpWall a move into the level sends, logged with
// "DXBUMP:" (docs/DEVELOPMENT.md, scripted runs). From wherever the run
// starts, 2 s in: a CapBumper spawned 150 units ahead of the player and 100
// up, then set 50 units over the floor under it; three Moves, each answer
// and the actor's height over the floor after it logged, the actor logging
// its BumpWall events: 100 units down (blocked partway by the floor), 100
// down again from where it stopped (blocked at its start), 60 up (free).
// Then the actor is destroyed and the run exits. What is compared between
// the engines: every line -- the original sends BumpWall for each move the
// level stops, with where the trace met the floor and its normal.
//=============================================================================
class BumpConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXBUMP: " $ S);
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
	local CapBumper B;
	local vector Spot;
	local float Floor;
	local bool Went;

	Spot = P.Location + Vector(P.Rotation) * 150 + vect(0, 0, 100);
	B = P.Spawn(class'CapBumper',,, Spot);
	if (B == None)
	{
		Say("no bumper spawned at " $ Spot);
		return;
	}
	Floor = FloorUnder(B);
	Spot.Z = Floor + B.CollisionHeight + 50;
	if (!B.SetLocation(Spot))
	{
		Say("the bumper would not go 50 over the floor");
		B.Destroy();
		return;
	}
	Say("bumper " $ B.Name $ " at " $ B.Location $ ", the floor at " $ Floor);

	Went = B.Move(vect(0, 0, -100));
	Say("100 down, blocked partway: " $ Went $ ", " $ Over(B, Floor) $ " over the floor");
	Went = B.Move(vect(0, 0, -100));
	Say("100 down again, blocked at the start: " $ Went $ ", " $ Over(B, Floor) $ " over the floor");
	Went = B.Move(vect(0, 0, 60));
	Say("60 up, free: " $ Went $ ", " $ Over(B, Floor) $ " over the floor");
	B.Destroy();
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
