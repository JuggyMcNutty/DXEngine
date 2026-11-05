//=============================================================================
// StateObjectConsole: an object new makes, sent to a state, logged with
// "DXSTATEOBJ:". 2 s in, from wherever the run starts: a CapStateObject made
// by new, its state's name, whether it is in Foo and whose Which() it runs,
// before and after GotoState('Foo'); then an exit. The original's
// InitExecution makes a state frame for an actor and the console alone
// (core-dll.md, events and probes): what an object new makes does here is
// what this logs.
//=============================================================================
class StateObjectConsole extends Console;

var float RunTime;
var bool bDone;

function Report(string When, CapStateObject O)
{
	Log("DXSTATEOBJ: " $ When $ ": state " $ O.GetStateName() $ ", in Foo " $ O.IsInState('Foo') $ ", Which " $ O.Which());
}

event Tick(float Delta)
{
	local CapStateObject O;

	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		O = new class'CapStateObject';
		Report("made", O);
		O.GotoState('Foo');
		Report("after GotoState('Foo')", O);
		Log("DXSTATEOBJ: done");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
