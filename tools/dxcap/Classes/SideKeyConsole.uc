//=============================================================================
// SideKeyConsole (bugs.md WIN.37): which keys the input gets
// for a shift, control or alt, and for keys held across a map load, in both
// engines. Every press and release the console's KeyEvent sees of Shift,
// Ctrl, Alt, their sides' own codes (LShift, RShift, LControl, RControl,
// UnknownA4, UnknownA5), W and Space is logged with the map's time and the
// map, "DXSIDEKEY:" in front. On Liberty Island, at OpenAt (14 s by default)
// the console opens the island again, then exits Seconds (26 by default)
// after the run began. Beside sidekeys.py's uinput keyboard (each modifier
// tapped from 6 s, Left Shift and W held from 12 s to 20 s; the game's
// window must have the focus).
//
// The original presses a key's code from its window messages alone
// (WM_KEYDOWN's virtual key, WinDrv.dll 0x1110a2a9: VK_SHIFT, VK_CONTROL,
// VK_MENU), and its poll of every code at the end of UpdateInput presses a
// key Windows holds down only when asked to reset (0x11109853-0x111098d3),
// which ReadInput's every-frame call is not (Engine.dll 0x103749eb, its 0)
// and UInput::ResetInput's is (0x10394a55, its 1): at a map load
// (UGameEngine::LoadMap 0x1038dff3), a net join, the focus. Expected: the
// original's taps Shift, Ctrl and Alt alone, and at the load a press of every
// key held (Shift, LShift, W, X, Space), its table of held keys cleared
// first, so the new pawn's bRun (Shift's Walking) and bDuck (X's Duck) are 1;
// DXEngine each tap's side code after the code of both (DXWidgets' SDL3
// window, sdl3_display_window.cpp:842-855), nothing at the load, and the new
// pawn's bRun and bDuck 0 while the keys are held. Each second after a load
// the pawn's bRun, bDuck and bFire are logged.
//=============================================================================
class SideKeyConsole extends Console;

var config float OpenAt;
var config float Seconds;

var float MapTime, RunTime, NextButtons;
var string CurrentMap;
var int Phase;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function bool Watched(EInputKey Key)
{
	return Key == IK_Shift || Key == IK_Ctrl || Key == IK_Alt
		|| Key == IK_LShift || Key == IK_RShift || Key == IK_LControl || Key == IK_RControl
		|| Key == IK_UnknownA4 || Key == IK_UnknownA5 || Key == IK_W || Key == IK_X || Key == IK_Space;
}

event bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
{
	if (Phase >= 1 && Phase < 99 && (Action == IST_Press || Action == IST_Release) && Watched(Key))
		Log("DXSIDEKEY: " $ GetEnum(enum'EInputKey', Key) $ " " $ GetEnum(enum'EInputAction', Action) $ " at " $ RunTime $ " (map " $ CurrentMap $ " " $ MapTime $ ")");
	return Super.KeyEvent(Key, Action, Delta);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float Limit, At;

	Super.Tick(Delta);
	RunTime += Delta;
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
		NextButtons = 0;
		if (Phase >= 1)
			Log("DXSIDEKEY: on " $ CurrentMap $ " at " $ RunTime);
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase == 2 && MapTime >= NextButtons)
	{
		Log("DXSIDEKEY: pawn " $ P $ " bRun " $ P.bRun $ " bDuck " $ P.bDuck $ " bFire " $ P.bFire $ " at " $ RunTime $ " (map " $ MapTime $ ")");
		NextButtons += 1.0;
	}

	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		Log("DXSIDEKEY: watching from " $ RunTime);
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	At = OpenAt;
	if (At <= 0)
		At = 14.0;
	if (Phase == 1 && RunTime > At)
	{
		Log("DXSIDEKEY: open 01_NYC_UNATCOIsland at " $ RunTime);
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Phase = 2;
	}

	Limit = Seconds;
	if (Limit <= 0)
		Limit = 26.0;
	if (RunTime > Limit)
	{
		Log("DXSIDEKEY: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}
