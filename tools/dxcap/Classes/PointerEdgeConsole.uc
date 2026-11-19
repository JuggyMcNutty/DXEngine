//=============================================================================
// PointerEdgeConsole (bugs.md EXT.39, EXT.40): the menus' pointer held at
// the screen's edge, and moved while hidden, logged with "DXPTR:" in front,
// in both engines. On the menu map, 1 s in, the main menu up and the root's
// size logged; then the root's GetCursorPos each quarter second it changed.
// Once the pointer has stood still for 0.75 s past half the root's width (a
// move right to the edge), its place is logged and it is hidden
// (ShowCursor(False)) for 4 s, through the move left that follows; shown
// again, its place logged: the edge's still where a hidden pointer does not
// move. At Seconds (22 by default) the lowest place it reached (a move down
// to the bottom edge), then an exit. The moves: tools/dxcap/timelines/
// pointer-edge.txt on DXEngine, or tools/dxcap/mouselook.py's uinput mouse
// (100 steps of 50 counts) in either engine, full screen, started once the
// menu is up. The original's root is the screen; DXEngine's is 4:3, its
// GetCursorPos the screen's place less the root's offset (120 at 1920x1080).
//=============================================================================
class PointerEdgeConsole extends Console;

var config float Seconds;

var float RunTime, NextLog, StillFor, HiddenAt, LastX, LastY, MaxY;
var int Phase;

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local float X, Y, Limit;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (DeusExPlayer(P) == None)
		return;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root == None)
		return;
	RunTime += Delta;
	Limit = Seconds;
	if (Limit <= 0)
		Limit = 22.0;

	if (Phase == 0)
	{
		if (RunTime < 1.0)
			return;
		Root.InvokeMenu(class'MenuMain');
		Root.GetCursorPos(LastX, LastY);
		MaxY = LastY;
		Log("DXPTR: the main menu up, the root " $ Root.width $ " by " $ Root.height $ ", the pointer at " $ LastX $ ", " $ LastY);
		Phase = 1;
		return;
	}

	Root.GetCursorPos(X, Y);
	if (X == LastX && Y == LastY)
		StillFor += Delta;
	else
		StillFor = 0;
	if (Y > MaxY)
		MaxY = Y;
	if (RunTime >= NextLog && (X != LastX || Y != LastY))
	{
		Log("DXPTR: at " $ X $ ", " $ Y $ " (" $ RunTime $ " s)");
		LastX = X;
		LastY = Y;
		NextLog = RunTime + 0.25;
	}

	if (Phase == 1 && X > Root.width * 0.5 && StillFor >= 0.75)
	{
		Log("DXPTR: the right edge at " $ X $ ", " $ Y $ "; hidden");
		Root.ShowCursor(False);
		HiddenAt = RunTime;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime - HiddenAt >= 4.0)
	{
		Log("DXPTR: shown again at " $ X $ ", " $ Y);
		Root.ShowCursor(True);
		Phase = 3;
	}
	else if (RunTime > Limit)
	{
		Log("DXPTR: the lowest place " $ MaxY $ "; exit");
		Phase = 9;
		P.ConsoleCommand("exit");
	}
}
