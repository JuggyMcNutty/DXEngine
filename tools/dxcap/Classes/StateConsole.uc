//=============================================================================
// StateConsole: state code's limits in a tick -- 2 s into whatever map the
// run starts on, a CapDyingActor spawned, whose state code destroys it and
// logs on, and a CapStateActor, whose state code bounces between two states
// by GotoState alone, its count logged each second for 4 s, then an exit.
// The original's ProcessState (Engine.dll 0x103e5b60) checks bDeleteMe
// before each bytecode and ends the tick's state code at the fifth state
// change, so the dying actor's lines after Destroy never run and the
// bouncer gains five entries a tick; DXEngine once ran on after Destroy and
// through any number of state changes, a bounce by GotoState never counted
// by its runaway guard (bugs.md E1.12). Each line starts "DXSTATE:".
//=============================================================================
class StateConsole extends Console;

var float RunTime, NextLog;
var int Step, Ticks;
var CapStateActor Bouncer;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 2.0)
	{
		P.Spawn(class'CapDyingActor');
		Bouncer = P.Spawn(class'CapStateActor');
		Log("DXSTATE: bouncer " $ Bouncer $ " spawned");
		NextLog = RunTime + 1.0;
		Ticks = 0;
		Step = 1;
	}
	else if (Step == 1)
	{
		Ticks++;
		if (RunTime >= NextLog)
		{
			Log("DXSTATE: bounces " $ Bouncer.Bounces $ " after " $ Ticks $ " ticks, " $ (Bouncer.Bounces / Ticks) $ " a tick, in state " $ Bouncer.GetStateName());
			NextLog += 1.0;
			if (RunTime > 6.0)
			{
				Log("DXSTATE: exiting");
				P.ConsoleCommand("exit");
				Step = 2;
			}
		}
	}
}
