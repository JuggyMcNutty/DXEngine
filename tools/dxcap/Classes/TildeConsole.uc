//=============================================================================
// TildeConsole (bugs.md ENG.41): what a typed ~ with Shift held does to the
// console, in both engines. Each character the console's KeyType is given
// outside its Typing state, each press and release its KeyEvent sees, and
// every quarter second the console's state, the player's bCheatsEnabled and
// the root window's mark of Shift, logged with "DXTILDE:". With
// DXCAP_CONFIG="Menu=True" the main menu is shown MenuAt seconds in (5 by
// default), so the same keys meet a modal window that holds the keyboard.
// Beside a uinput keyboard that holds Left Shift and taps the grave key.
//=============================================================================
class TildeConsole extends Console;

var config bool Menu;
var config float MenuAt;

var float RunTime, LogTime;
var bool bMenuShown;

event bool KeyType(EInputKey Key)
{
	local bool bTook;
	local name Before;

	Before = GetStateName();
	bTook = Super.KeyType(Key);
	Log("DXTILDE: KeyType " $ int(Key) $ " state " $ Before $ " -> " $ GetStateName() $ " took " $ bTook);
	return bTook;
}

event bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
{
	local bool bTook;
	local name Before;

	if (Action != IST_Press && Action != IST_Release)
		return Super.KeyEvent(Key, Action, Delta);
	Before = GetStateName();
	bTook = Super.KeyEvent(Key, Action, Delta);
	Log("DXTILDE: KeyEvent " $ int(Key) $ " action " $ int(Action) $ " state " $ Before $ " -> " $ GetStateName() $ " took " $ bTook);
	return bTook;
}

event Tick(float Delta)
{
	local DeusExPlayer P;
	local float At;

	Super.Tick(Delta);
	if (Viewport == None)
		return;
	P = DeusExPlayer(Viewport.Actor);
	if (P == None || P.rootWindow == None)
		return;
	RunTime += Delta;
	At = MenuAt;
	if (At <= 0)
		At = 5.0;
	if (Menu && !bMenuShown && RunTime > At)
	{
		bMenuShown = true;
		P.ShowMainMenu();
		Log("DXTILDE: main menu shown");
	}
	if (RunTime - LogTime < 0.25)
		return;
	LogTime = RunTime;
	Log("DXTILDE: t=" $ int(RunTime * 4) / 4.0 $ " state=" $ GetStateName() $ " cheats=" $ P.bCheatsEnabled $ " shift=" $ P.rootWindow.IsKeyDown(IK_Shift) $ " typed=" $ TypedStr);
}

defaultproperties
{
}
