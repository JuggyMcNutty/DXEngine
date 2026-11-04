//=============================================================================
// WheelConsole: the wheel's presses as the game gets them -- each
// MouseWheelUp and MouseWheelDown the console's KeyEvent sees, logged with
// "DXWHEEL:" and the seconds since the run began -- for a run whose mouse
// turns its wheel: a uinput mouse's high-resolution wheel, eight
// REL_WHEEL_HI_RES of 15 a notch, or a classic notch. On whatever map the
// run starts on, in play, the cursor the game's; an exit after Seconds (20
// by default).
//=============================================================================
class WheelConsole extends Console;

var config float Seconds;
var float RunTime;
var bool bExited;

event bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
{
	if (Key == IK_MouseWheelUp || Key == IK_MouseWheelDown)
		Log("DXWHEEL: " $ GetEnum(enum'EInputKey', Key) $ " " $ GetEnum(enum'EInputAction', Action) $ " at " $ RunTime);
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
