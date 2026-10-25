//=============================================================================
// TeleportConsole: what a SetLocation's touches see, logged with "DXTELE:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: two CapTeleported actors spawned
// over the player, Target 200 up and Mover 400 up, and a third, Rider, set
// on Mover as its base; Mover moved onto Target with SetLocation, then away
// again, each move's result and where Mover then stands logged, with
// Target's Touch and UnTouch, Rider's BaseChange and where each saw the
// other; and an exit. What is compared between the engines: every line,
// exactly.
//=============================================================================
class TeleportConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXTELE: " $ S);
}

function Probes()
{
	local CapTeleported Target, Mover, Rider;
	local PlayerPawn P;
	local bool B;

	P = Viewport.Actor;
	Target = P.Spawn(class'CapTeleported',, 'Target', P.Location + vect(0, 0, 200));
	Mover = P.Spawn(class'CapTeleported',, 'Mover', P.Location + vect(0, 0, 400));
	Rider = P.Spawn(class'CapTeleported',, 'Rider', P.Location + vect(0, 0, 440));
	if (Target == None || Mover == None || Rider == None)
	{
		Say("not all spawned: " $ Target $ " " $ Mover $ " " $ Rider);
		return;
	}
	Rider.SetBase(Mover);
	Say("target at " $ Target.Location $ ", mover at " $ Mover.Location $ ", rider at " $ Rider.Location $ " on " $ Rider.Base);
	B = Mover.SetLocation(Target.Location + vect(10, 0, 0));
	Say("moved onto target: " $ B $ ", mover at " $ Mover.Location $ ", rider at " $ Rider.Location $ " on " $ Rider.Base);
	B = Mover.SetLocation(Target.Location + vect(500, 0, 0));
	Say("moved away: " $ B $ ", mover at " $ Mover.Location);
	Target.Destroy();
	Mover.Destroy();
	Rider.Destroy();
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes();
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
