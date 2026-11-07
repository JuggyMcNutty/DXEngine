//=============================================================================
// LogTimeoutConsole: a text log line added late in a frame, and its time
// out, logged with "DXLOGTIME:" (bugs.md EXT.26).
//
// The original's AddLog (XTextLogWindow::AddLog, Extension.dll 0x100476a0)
// gives a line the time out plus what of the frame has gone (GetTickOffset),
// which the windows' next tick, by the real time since the last, takes back:
// a line lasts its whole time out from when it came.
//
// From the menu map, 3 s in, a CapLogLine made on the root. At 3.5 s, from
// the console's tick, outside the windows', the frame held until a quarter
// second has gone since the windows ticked (GetTickOffset), then a line
// added with a time out of 0.12 s: the log's script logs at its next tick
// whether the line is still there. At 4.5 s whether it is there, then an
// exit.
//=============================================================================
class LogTimeoutConsole extends Console;

var float RunTime;
var int Step;
var CapLogLine LogWin;

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local Color White;
	local int i;

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
		LogWin = CapLogLine(Root.NewChild(class'CapLogLine'));
		LogWin.SetPos(0, 0);
		LogWin.SetSize(200, 20);
		Log("DXLOGTIME: made the log");
		break;
	case 1:
		if (RunTime < 3.5)
			return;
		while (i < 2000000 && LogWin.GetTickOffset() < 0.25)
			i++;
		Log("DXLOGTIME: a quarter second gone since the windows ticked " $ (LogWin.GetTickOffset() >= 0.25));
		White.R = 255;
		White.G = 255;
		White.B = 255;
		LogWin.SetTextTimeout(0.12);
		LogWin.AddLog("LogTimeoutConsole's line", White);
		LogWin.bArmed = True;
		Log("DXLOGTIME: line added: " $ LogWin.Report());
		break;
	case 2:
		if (RunTime < 4.5)
			return;
		Log("DXLOGTIME: at 4.5 s: " $ LogWin.Report());
		Log("DXLOGTIME: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
