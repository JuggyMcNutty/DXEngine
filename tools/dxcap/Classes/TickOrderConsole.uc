//=============================================================================
// TickOrderConsole: the order of a window's tick, the same in both engines
// (docs/DEVELOPMENT.md, scripted runs; bugs.md EXT.23): the original's walk
// of the windows (XRootWindow::TickTree, Extension.dll 0x1003bfc0) runs each
// window's own tick, then its script's Tick, then its timers, then its
// children. From the menu map, 3 s in, three windows of the package's own
// are put on the root, each with its script's Tick on and a CapTickChild
// under it: a computer window (CapTickComputer), which logs its first tick
// and its ComputerStart as they come; a button (CapTickButton) and a text log
// (CapTickLog). At 4 s the button is pressed and the log given a line with
// no time out, each with timers due in the next tick, from the console's
// tick, outside the windows'; each logs what its next ticks see. Each line
// is logged with "DXTICK:"; an exit at 5 s.
//=============================================================================
class TickOrderConsole extends Console;

var float RunTime;
var int Step;
var CapTickComputer ComputerProbe;
var CapTickButton ButtonProbe;
var CapTickLog LogProbe;

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
	if (Root == None)
		return;

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		ComputerProbe = CapTickComputer(Root.NewChild(class'CapTickComputer'));
		ComputerProbe.SetPos(0, 0);
		ComputerProbe.SetSize(70, 20);
		ButtonProbe = CapTickButton(Root.NewChild(class'CapTickButton'));
		ButtonProbe.SetPos(0, 30);
		ButtonProbe.SetSize(70, 20);
		LogProbe = CapTickLog(Root.NewChild(class'CapTickLog'));
		LogProbe.SetPos(0, 60);
		LogProbe.SetSize(140, 20);
		Log("DXTICK: made the computer window, the button and the text log");
		break;
	case 1:
		if (RunTime < 4.0)
			return;
		ButtonProbe.Arm();
		LogProbe.Arm();
		break;
	case 2:
		if (RunTime < 5.0)
			return;
		Log("DXTICK: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
