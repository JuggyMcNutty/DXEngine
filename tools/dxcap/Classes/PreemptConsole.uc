//=============================================================================
// PreemptConsole: a GotoState from within EndState or BeginState, logged with
// "DXPREEMPT:" from wherever the run starts (docs/DEVELOPMENT.md, scripted
// runs). 2 s in a CapPreempter is spawned and sent to Idle, then to Goal
// with its EndState set to divert it to Diverted: the original ends in
// Diverted, the Goal change preempted (Core.dll UObject::GotoState's
// InEndState and StateChanged flags), where DXEngine once sent EndState
// again and then put the actor in Goal (bugs.md E5a.3). 0.5 s later it is
// sent to Idle and to Goal with its BeginState set to bounce it to
// Bounced: both engines end in Bounced, Goal's Begin label never run. Then
// an exit.
//=============================================================================
class PreemptConsole extends Console;

var float RunTime;
var int Phase;
var CapPreempter E;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase == 0 && RunTime > 2.0)
	{
		E = P.Spawn(class'CapPreempter',,, P.Location + vector(P.Rotation) * 100);
		if (E == None)
		{
			Log("DXPREEMPT: no room for the actor");
			Phase = 8;
			return;
		}
		E.GotoState('Idle');
		Log("DXPREEMPT: in " $ E.GetStateName());
		E.bDivert = True;
		E.GotoState('Goal');
		Log("DXPREEMPT: after GotoState Goal with a divert in EndState: in " $ E.GetStateName());
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 2.5)
	{
		E.GotoState('Idle');
		Log("DXPREEMPT: in " $ E.GetStateName());
		E.bBounce = True;
		E.GotoState('Goal');
		Log("DXPREEMPT: after GotoState Goal with a bounce in BeginState: in " $ E.GetStateName());
		Phase = 2;
	}
	else if ((Phase == 2 && RunTime > 3.0) || (Phase == 8 && RunTime > 3.0))
	{
		Log("DXPREEMPT: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
