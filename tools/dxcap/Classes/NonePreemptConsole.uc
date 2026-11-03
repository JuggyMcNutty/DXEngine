//=============================================================================
// NonePreemptConsole: a GotoState to no
// state, or to a state the class has not, from within EndState, logged with
// "DXNONEPREEMPT:". 2 s in: a CapNonePreempter spawned and sent to Idle,
// then to Goal with bToNone set; 0.5 s later the same with bToMissing; 0.5 s
// later an exit. The original's UObject::GotoState (Core.dll 0x1012e8f0)
// returns without setting RF_StateChanged when it ends in no state (the
// name None or one not found), so the outer GotoState('Goal') goes on: Goal,
// its BeginState and its Begin. DXEngine's (UObject.cpp, GotoState) sets
// StateChanged at its end whatever the state, so the outer one is preempted
// and the actor stays in no state.
//=============================================================================
class NonePreemptConsole extends Console;

var float RunTime;
var int Phase;
var CapNonePreempter E;

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		E = Viewport.Actor.Spawn(class'CapNonePreempter',,, Viewport.Actor.Location + vect(0, 0, 200));
		if (E == None)
		{
			Log("DXNONEPREEMPT: no room for the actor");
			Phase = 8;
			return;
		}
		E.GotoState('Idle');
		E.bToNone = True;
		E.GotoState('Goal');
		Log("DXNONEPREEMPT: after GotoState Goal with GotoState None in EndState: in " $ E.GetStateName());
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 2.5)
	{
		E.GotoState('Idle');
		E.bToMissing = True;
		E.GotoState('Goal');
		Log("DXNONEPREEMPT: after GotoState Goal with GotoState NoSuchState in EndState: in " $ E.GetStateName());
		Phase = 2;
	}
	else if ((Phase == 2 && RunTime > 3.0) || (Phase == 8 && RunTime > 3.0))
	{
		Log("DXNONEPREEMPT: done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 9;
	}
}
