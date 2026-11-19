//=============================================================================
// FocusLossConsole (bugs.md ENG.40): what the player's input holds as the
// game's window loses the focus and takes it back, in both engines. On
// Liberty Island, opened from wherever the run starts, each half second from
// 3 s in the pawn's bFire, bAltFire, bRun and bDuck are logged, "DXFOCUS:" in
// front, and each press and release the console's KeyEvent sees of LeftMouse,
// Shift, LShift, X, A and Q; it exits Seconds (30 by default) after the island
// began. Beside tools/dxcap/focusheld.py, which holds Left Shift and X (keys)
// or the left mouse button (mouse, the run full screen) in the game's window,
// gives the focus to a window of its own and, some seconds later, back.
// With tools/dxcap/timelines/held-azerty.txt on DXEngine's SDL windows, a
// timeline's key at Q's place held over that change under the AZERTY
// family, where it is A: the key the focus's return presses again is A.
//
// The original resets its input at both (WinDrv.dll WM_KILLFOCUS 0x1110a0a3,
// WM_SETFOCUS 0x1110a1ab; UInput::ResetInput, Engine.dll 0x10394910): the
// table of keys held cleared and every input property of the player's
// zeroed, no release sent; then, the focus back, its poll presses each key
// Windows holds down (0x11109853-0x111098d3).
//=============================================================================
class FocusLossConsole extends Console;

var config float Seconds;

var float MapTime, NextLine;
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

event bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
{
	if (Phase == 1 && (Action == IST_Press || Action == IST_Release)
		&& (Key == IK_LeftMouse || Key == IK_Shift || Key == IK_LShift || Key == IK_X || Key == IK_A || Key == IK_Q))
		Log("DXFOCUS: " $ GetEnum(enum'EInputKey', Key) $ " " $ GetEnum(enum'EInputAction', Action) $ " at " $ MapTime);
	return Super.KeyEvent(Key, Action, Delta);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float Limit;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

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
		if (MapTime < 3.0)
			return;
		Log("DXFOCUS: watching");
		NextLine = MapTime;
		Phase = 1;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	if (MapTime >= NextLine)
	{
		Log("DXFOCUS: bFire " $ P.bFire $ " bAltFire " $ P.bAltFire $ " bRun " $ P.bRun $ " bDuck " $ P.bDuck $ " at " $ MapTime);
		NextLine += 0.5;
	}

	Limit = Seconds;
	if (Limit <= 0)
		Limit = 30.0;
	if (MapTime > Limit)
	{
		Log("DXFOCUS: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

defaultproperties
{
}
