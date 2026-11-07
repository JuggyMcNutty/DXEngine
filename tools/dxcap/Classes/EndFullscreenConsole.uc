//=============================================================================
// EndFullscreenConsole: the console's endfullscreen in a window and full
// screen (bugs.md WIN.17), logged with "DXENDFULL:". On the map the run
// starts on, 4 s in: the root's size, then endfullscreen; 3 s later the
// root's size, then togglefullscreen; 3 s later the root's size, then
// endfullscreen again; 3 s later the root's size, then an exit 2 s later.
// Run in a window with DXCAP_INI giving full screen the display's size
// ("WinDrv.WindowsClient:FullscreenViewportX=2560
// WinDrv.WindowsClient:FullscreenViewportY=1440"), so that the window's and
// full screen's sizes tell the two apart. The original's window takes
// ENDFULLSCREEN in any state, after every other taker
// (UWindowsViewport::Exec, WinDrv.dll 0x11106dcf), its EndFullscreen logging
// EndFullscreen and sizing the viewport in a window at the client's
// windowed size (0x1110d380): in a window the window's own size, full
// screen the toggle's way back. Compared: the lines, and each engine's
// EndFullscreen and AttemptFullscreen lines in order (docs/DEVELOPMENT.md,
// scripted runs).
//=============================================================================
class EndFullscreenConsole extends Console;

var float StepTime;
var int Phase;

function Report(PlayerPawn P, string What)
{
	local DeusExRootWindow Root;

	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root != None)
		Log("DXENDFULL: " $ What $ ": root " $ int(Root.width) $ " x " $ int(Root.height));
	else
		Log("DXENDFULL: " $ What);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	StepTime += Delta;

	if (Phase == 0 && StepTime > 4.0)
	{
		Report(P, "step 0, endfullscreen");
		P.ConsoleCommand("endfullscreen");
		StepTime = 0;
		Phase = 1;
	}
	else if (Phase == 1 && StepTime > 3.0)
	{
		Report(P, "step 1, togglefullscreen");
		P.ConsoleCommand("togglefullscreen");
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 3.0)
	{
		Report(P, "step 2, endfullscreen");
		P.ConsoleCommand("endfullscreen");
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 3.0)
	{
		Report(P, "step 3");
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 2.0)
	{
		Log("DXENDFULL: exiting");
		P.ConsoleCommand("exit");
		Phase = 5;
	}
}
