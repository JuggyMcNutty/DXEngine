//=============================================================================
// SmoothConsole: the script's MoveSmooth, logged with "DXSMOOTH:"
// (docs/DEVELOPMENT.md, scripted runs). From wherever the run starts, 2 s
// in: a CapBumper spawned 150 units ahead of the player and 100 up, then
// set 50 units over the floor under it; three MoveSmooths, each answer and
// the actor's place after it logged, the actor logging its BumpWall and
// HitWall events: 100 along +X and 100 down (the floor met halfway, the
// rest slid along it), 10 down from where it rests (blocked at the start),
// 60 up (free). Then the actor is destroyed and the run exits. What is
// compared between the engines: every line -- the original's move is held
// off 2 units short of the floor, and its answer is whether the first move
// went at all.
//=============================================================================
class SmoothConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXSMOOTH: " $ S);
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

function Probes(PlayerPawn P)
{
	local CapBumper B;
	local vector Spot, Start;
	local float Floor;
	local bool Went;

	Spot = P.Location + Vector(P.Rotation) * 150 + vect(0, 0, 100);
	B = P.Spawn(class'CapBumper',,, Spot);
	if (B == None)
	{
		Say("no actor spawned at " $ Spot);
		return;
	}
	Floor = FloorUnder(B);
	Spot.Z = Floor + B.CollisionHeight + 50;
	if (!B.SetLocation(Spot))
	{
		Say("the actor would not go 50 over the floor");
		B.Destroy();
		return;
	}
	Start = B.Location;
	Say("actor " $ B.Name $ " at " $ Start $ ", the floor at " $ Floor);

	Went = B.MoveSmooth(vect(100, 0, -100));
	Say("100 along +X and 100 down: " $ Went $ ", moved " $ (B.Location - Start) $ ", " $ (B.Location.Z - B.CollisionHeight - Floor) $ " over the floor");
	Start = B.Location;
	Went = B.MoveSmooth(vect(0, 0, -10));
	Say("10 down from the rest: " $ Went $ ", moved " $ (B.Location - Start));
	Start = B.Location;
	Went = B.MoveSmooth(vect(0, 0, 60));
	Say("60 up, free: " $ Went $ ", moved " $ (B.Location - Start));
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
