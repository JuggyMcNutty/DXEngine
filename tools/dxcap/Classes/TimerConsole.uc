//=============================================================================
// TimerConsole: a window's timers, the same in both engines
// (docs/DEVELOPMENT.md, scripted runs). From the menu map, the main menu
// shown, a TimerLogWindow is put on it and adds its timers: one of 0.5 s,
// one looping every millisecond, one looping at 0, two of 0.3 s added in
// turn, and one with no function name; each call they make is logged
// ("DXTIMER:") as it comes. Then an exit.
//=============================================================================
class TimerConsole extends Console;

var float RunTime;
var int Step;
var TimerLogWindow Holder;

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		break;
	case 1:
		if (RunTime < 4.0)
			return;
		Holder = TimerLogWindow(Root.GetTopWindow().NewChild(class'TimerLogWindow'));
		Holder.SetPos(10, 10);
		Holder.SetSize(100, 100);
		Holder.Start();
		break;
	case 2:
		if (RunTime < 7.0)
			return;
		Log("DXTIMER: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
