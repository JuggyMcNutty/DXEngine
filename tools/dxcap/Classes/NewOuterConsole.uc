//=============================================================================
// NewOuterConsole: the outer of an object new makes, logged with
// "DXNEWOUTER:".
//
// The original's execNew (Core.dll 0x1013d9f0) starts the outer as the
// calling object and steps the argument into it: left out (new class'X') the
// object is made in the caller, with None in the transient package. 2 s in,
// from wherever the run starts: a CapNewProbe made by the console with no
// outer, with None and with Self, each logged as its path and its Outer;
// then a CapNewMaker spawned, which does the same in its first Tick; then an
// exit.
//=============================================================================
class NewOuterConsole extends Console;

var float RunTime;
var int Phase;

function Report(string Which, Object O)
{
	Log("DXNEWOUTER: the console's " $ Which $ ": " $ string(O) $ ", its Outer " $ O.Outer);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Report("new", new class'CapNewProbe');
		Report("new(None)", new(None) class'CapNewProbe');
		Report("new(Self)", new(Self) class'CapNewProbe');
		Viewport.Actor.Spawn(class'CapNewMaker');
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXNEWOUTER: done");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 2;
	}
}
