//=============================================================================
// KeyOrderConsole (bugs.md ENG.61): whether the console sees a key while a
// window holds the keyboard, in both engines. On Liberty Island, opened from
// wherever the run starts, the main menu is shown 5 s in (ShowMainMenu, as
// its key shows it); from then each half second the top window's class is
// logged, "DXKEYORDER:" in front, and each press and release the console's
// KeyEvent sees of Shift, LShift and X; it exits Seconds (30 by default)
// after the island began. Beside tools/dxcap/focusheld.py keys, which holds
// Left Shift and X in the game's window.
//
// The original asks the console first of every key (UEngine::InputEvent,
// Engine.dll 0x103814e0), then its input, whose root window takes every key
// while a window holds the keyboard: the console logs the keys, the menu up.
//=============================================================================
class KeyOrderConsole extends Console;

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

function string TopOf(PlayerPawn P)
{
	local DeusExRootWindow Root;

	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root == None || Root.GetTopWindow() == None)
		return "None";
	return string(Root.GetTopWindow().Class);
}

function Note(EInputKey Key, EInputAction Action)
{
	if (Phase >= 1 && Phase != 99 && (Action == IST_Press || Action == IST_Release)
		&& (Key == IK_Shift || Key == IK_LShift || Key == IK_X))
		Log("DXKEYORDER: " $ GetEnum(enum'EInputKey', Key) $ " " $ GetEnum(enum'EInputAction', Action) $ " at " $ MapTime $ ", the top window " $ TopOf(Viewport.Actor) $ ", the console's state " $ GetStateName());
}

event bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
{
	Note(Key, Action);
	return Super.KeyEvent(Key, Action, Delta);
}

// A state's KeyEvent stands before the class's own: the states the stock
// console has one in, each noting the key and handing it on.
state Typing
{
	function bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
	{
		Note(Key, Action);
		return Super.KeyEvent(Key, Action, Delta);
	}
}

state Menuing
{
	function bool KeyEvent(EInputKey Key, EInputAction Action, FLOAT Delta)
	{
		Note(Key, Action);
		return Super.KeyEvent(Key, Action, Delta);
	}
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
		if (MapTime < 5.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		Log("DXKEYORDER: the main menu asked for at " $ MapTime);
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
		Log("DXKEYORDER: the top window " $ TopOf(P) $ ", the console's state " $ GetStateName() $ " at " $ MapTime);
		NextLine += 0.5;
	}

	Limit = Seconds;
	if (Limit <= 0)
		Limit = 30.0;
	if (MapTime > Limit)
	{
		Log("DXKEYORDER: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

defaultproperties
{
}
