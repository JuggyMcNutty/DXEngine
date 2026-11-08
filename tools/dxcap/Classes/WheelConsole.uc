//=============================================================================
// WheelConsole: the wheel's presses as the game gets them -- each
// MouseWheelUp and MouseWheelDown the console's KeyEvent sees, and each
// MouseW axis event with its delta, logged with "DXWHEEL:" and the seconds
// since the run began -- for a run whose mouse turns its wheel: a uinput
// mouse's high-resolution wheel, eight REL_WHEEL_HI_RES of 15 a notch, or a
// classic notch, or tools/dxcap/timelines/wheel.txt's notches. On whatever
// map the run starts on, in play, the cursor the game's; an exit after
// Seconds (20 by default). The original's window gives MouseW a
// WM_MOUSEWHEEL's delta, 120 a notch, before its press and release
// (WinDrv.dll 0x1110b2bd; bugs.md ENG.38).
//=============================================================================
class WheelConsole extends Console;

var config float Seconds;
var float RunTime;
var bool bExited;

event bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
{
	if (Key == IK_MouseWheelUp || Key == IK_MouseWheelDown)
		Log("DXWHEEL: " $ GetEnum(enum'EInputKey', Key) $ " " $ GetEnum(enum'EInputAction', Action) $ " at " $ RunTime);
	else if (Key == IK_MouseW)
		Log("DXWHEEL: " $ GetEnum(enum'EInputKey', Key) $ " " $ GetEnum(enum'EInputAction', Action) $ " " $ Delta $ " at " $ RunTime);
	return Super.KeyEvent(Key, Action, Delta);
}

event Tick(float Delta)
{
	local float Limit;

	Super.Tick(Delta);
	RunTime += Delta;
	Limit = Seconds;
	if (Limit <= 0)
		Limit = 20.0;
	if (!bExited && RunTime > Limit && Viewport != None && Viewport.Actor != None)
	{
		Log("DXWHEEL: exiting at " $ RunTime);
		Viewport.Actor.ConsoleCommand("exit");
		bExited = true;
	}
}
