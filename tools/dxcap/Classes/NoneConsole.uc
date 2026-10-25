//=============================================================================
// NoneConsole: what a None context yields, logged with "DXNONE:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in, with Actor, Pawn, Mover and
// DeusExLevelInfo variables left None: an array element written through
// None (as AutoTurret's gun.MultiSkins[2] = None), elements read through
// None into variables set before (an object, an int, a vector, a string)
// and compared; a member of each type read through None (a bool, bytes, an
// int, a float, a name, a string, a vector, a rotator, an object) and used
// in expressions; a switch on a byte, a name and a string through None; a
// struct copied from one through None, compared with it, its members read
// through it, and nested contexts; a script and two native calls through
// None; a native's optional argument through None (Trace's start, Spawn's
// location); and an exit. What is compared between the engines: every
// line, exactly. The probes are CapNoneProbe's, an actor.
//=============================================================================
class NoneConsole extends Console;

var float RunTime;
var bool bDone;
var CapNoneProbe Probe;

function Say(string S)
{
	Log("DXNONE: " $ S);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probe = Viewport.Actor.Spawn(class'CapNoneProbe',,, Viewport.Actor.Location + vect(0, 0, 100));
		if (Probe == None)
			Say("no probe spawned");
		else
		{
			Probe.Probes();
			Probe.Destroy();
		}
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
