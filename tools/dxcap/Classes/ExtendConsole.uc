//=============================================================================
// ExtendConsole: a state extending another, logged with "DXEXTEND:" from
// wherever the run starts (docs/DEVELOPMENT.md, scripted runs). 2 s in a
// CapExtender is spawned and sent to its state Upper, which extends Lower
// and has no code or functions of its own: Lower's Begin label runs (and
// Other after it, the code falling through), the actor's state name read;
// 0.5 s later Hello is called on it (Lower's, not the class's), then it goes
// to Upper's Other label; 0.5 s later to Lower itself and Hello again; then
// an exit. The original looks a label and a function up in the state and
// the states above it (Core.dll UObject::GotoLabel, FindFunction); DXEngine
// once looked only at states of the same name up the classes, so Upper's
// Begin found nothing and Hello was the class's (bugs.md E1.13).
//=============================================================================
class ExtendConsole extends Console;

var float RunTime;
var int Phase;
var CapExtender E;

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
		E = P.Spawn(class'CapExtender',,, P.Location + vector(P.Rotation) * 100);
		if (E == None)
		{
			Log("DXEXTEND: no room for the actor");
			Phase = 8;
			return;
		}
		E.GotoState('Upper');
		Log("DXEXTEND: sent to Upper: state " $ E.GetStateName() $ ", in Lower " $ E.IsInState('Lower') $ ", in Upper " $ E.IsInState('Upper'));
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 2.5)
	{
		E.Hello();
		E.GotoState('Upper', 'Other');
		Log("DXEXTEND: sent to Upper's Other: state " $ E.GetStateName());
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 3.0)
	{
		E.GotoState('Lower');
		Log("DXEXTEND: sent to Lower: state " $ E.GetStateName() $ ", in Upper " $ E.IsInState('Upper'));
		E.Hello();
		Phase = 3;
	}
	else if ((Phase == 3 && RunTime > 3.5) || (Phase == 8 && RunTime > 3.0))
	{
		Log("DXEXTEND: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
