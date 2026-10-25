//=============================================================================
// BubbleConsole: actors spawned under water, logged with "DXBUBBLE:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, 2 s in, in the water 80 past the pier's edge
// RestConsole and SwimConsole find, 100 under its line: an AirBubble, as a
// swimming player spawns before its eyes, a plain Effects and a Rat spawned
// there, each one's zone, physics, velocity and height logged at its spawn
// and every 0.25 s for 3 s (an actor gone, as a bubble that leaves the water
// is, says so); and an exit. What is compared between the engines: the
// physics, and whether the bubble rises.
//=============================================================================
class BubbleConsole extends Console;

var float RunTime, NextLog, Spawned;
var int Phase;
var AirBubble Bubble;
var Effects Plain;
var Rat Pawn;

function Say(string S)
{
	Log("DXBUBBLE: " $ S);
}

function string Of(string What, Actor A)
{
	if (A == None)
		return What $ " gone";
	return What $ " in " $ A.Region.Zone.Name $ " water " $ A.Region.Zone.bWaterZone $ " physics " $ GetEnum(enum'EPhysics', A.Physics) $ " velocity z " $ A.Velocity.Z $ " at z " $ A.Location.Z;
}

function Report(string When)
{
	Say(When $ ": " $ Of("bubble", Bubble) $ "; " $ Of("effects", Plain) $ "; " $ Of("rat", Pawn));
}

event Tick(float Delta)
{
	local vector Spot;

	Super.Tick(Delta);
	if (Phase == 2 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		// SwimConsole's water line, 80 past the edge, 100 under it
		Spot = vect(-4760.569824, 9920.061523, -451.987793);
		Bubble = Viewport.Actor.Spawn(class'AirBubble',,, Spot);
		Plain = Viewport.Actor.Spawn(class'Effects',,, Spot);
		Pawn = Viewport.Actor.Spawn(class'Rat',,, Spot);
		Report("spawned");
		Spawned = RunTime;
		NextLog = RunTime + 0.25;
		Phase = 1;
	}
	else if (Phase == 1 && RunTime >= NextLog)
	{
		Report("at " $ int((RunTime - Spawned) * 100 + 0.5) / 100.0 $ " s");
		NextLog += 0.25;
		if (RunTime - Spawned >= 3.0)
		{
			Say("done, exiting");
			Viewport.Actor.ConsoleCommand("exit");
			Phase = 2;
		}
	}
}
