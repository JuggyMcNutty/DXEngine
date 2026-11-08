//=============================================================================
// WindowSizeConsole: the size the window is dragged to, kept as the
// client's windowed size and saved at once (bugs.md WIN.18), logged with
// "DXWINSIZE:". On the map the run starts on, every second from 2 s: the
// client's WindowedViewportX and Y (GET of ini:Engine.Engine.ViewportManager,
// the class's defaults, which the original's SaveConfig reads the ini back
// into), the viewport's size (GETCURRENTRES) and the root's; at 26 s
// togglefullscreen, at 30 s togglefullscreen again, the way back from full
// screen; then on until the run's end, with no exit of its own. Run beside
// tools/dxcap/maximize.py --size 1001x701, a drag of the frame to an odd
// width, and with DXCAP_KILL=1, the run ended by a kill, so the run's
// ini-after.ini shows what was written before any exit. The original's
// WM_SIZE in a window (WinDrv.dll 0x1110a033) resizes its viewport, which
// makes the width even (0x1110d9a8) and sizes the window to it, keeps the
// size as the client's windowed size and saves the client's config, the
// ini written at once (0x1110e2f3-0x1110e35b): 1002 by 701 from the drag on,
// the way back from full screen at it, and the ini holding it after a kill.
// The original's log is written only as it exits: its lines come from a
// run without DXCAP_KILL, its ini after a kill from one with it. Compared:
// the lines, and WindowedViewportX and Y in each run's ini-after.ini
// (docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class WindowSizeConsole extends Console;

var float RunTime;
var float NextReport;
var int Phase;

function Report(PlayerPawn P, string What)
{
	local DeusExRootWindow Root;
	local string Line;

	Line = "DXWINSIZE: " $ What $ " at " $ int(RunTime) $ " s: WindowedViewportX "
		$ P.ConsoleCommand("get ini:Engine.Engine.ViewportManager WindowedViewportX")
		$ ", WindowedViewportY " $ P.ConsoleCommand("get ini:Engine.Engine.ViewportManager WindowedViewportY")
		$ ", the viewport " $ P.ConsoleCommand("getcurrentres");
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root != None)
		Line = Line $ ", root " $ int(Root.width) $ " x " $ int(Root.height);
	Log(Line);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Phase == 0 && RunTime > 26.0)
	{
		Report(P, "togglefullscreen");
		P.ConsoleCommand("togglefullscreen");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 30.0)
	{
		Report(P, "togglefullscreen back");
		P.ConsoleCommand("togglefullscreen");
		Phase = 2;
	}
	else if (RunTime > 2.0 && RunTime >= NextReport)
	{
		Report(P, "size");
		NextReport = int(RunTime) + 1;
	}
}
