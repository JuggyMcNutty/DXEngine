//=============================================================================
// ResetAllConsole (bugs.md ENG.29's other screens): ResetConsole's run on
// the other screens with a Reset to Defaults button (DeusEx.int's
// actionButtons with AB_Reset): Controls, Game Options, Colors, Brightness,
// Customize Keys, and multiplayer's Player Setup and Host Game. From the
// menu map, the main menu shown at 3 s, each screen is opened in turn: its
// choices logged (value, text, the GET of a config setting) as opened, its
// ResetToDefaults called as the button calls it (MenuUIWindow's
// ProcessActionButton, AB_Reset), the choices logged again; the settings
// screens (the first four) then closed with OK as the player saves them, a
// box answered OK, opened again and logged once more; the others closed with
// Cancel, as nothing here should start a game or a server. Customize Keys
// logs its list's rows, function and keys, which its ResetToDefaults builds
// again from KEYNAME and KEYBINDING of each code 0 to 254. Each line starts
// "DXRESETALL:", then an exit.
//
// Expected: every line alike in both engines. The choices on these screens
// set the player's and the HUD's variables in script, or SET a class's
// property through the generic import (Engine.cpp:3227-3317, the int import
// taking a slider's "16.000000" as 16, as Core.dll 0x10165f00 does);
// Brightness's sends set ini:Engine.Engine.ViewportManager Brightness and
// FLUSH, the client's held value the original's. Customize Keys' rows alike
// unless a binding names codes 0xF0 to 0xF3 (DXEngine's KEYNAME JoyPovUp to
// JoyPovRight, the original's UnknownF0 to UnknownF3).
//=============================================================================
class ResetAllConsole extends Console;

var float RunTime;
var float Wait;
var int Phase;
var int ScreenIndex;

function class<MenuUIScreenWindow> ScreenClass(int i)
{
	switch (i)
	{
	case 0: return class'MenuScreenControls';
	case 1: return class'MenuScreenOptions';
	case 2: return class'MenuScreenAdjustColors';
	case 3: return class'MenuScreenBrightness';
	case 4: return class'MenuScreenCustomizeKeys';
	case 5: return class'MenuScreenPlayerSetup';
	case 6: return class'MenuScreenHostGame';
	}
	return None;
}

// The first four are saved with OK, the others cancelled
function bool SavedWithOK(int i)
{
	return i <= 3;
}

function LogChoices(PlayerPawn P, MenuUIScreenWindow S, string Label)
{
	local Window C;
	local MenuUIChoice Choice;
	local MenuUIChoiceEnum E;
	local MenuScreenCustomizeKeys K;
	local string Line;
	local int i, Row;

	Log("DXRESETALL: " $ S.Class.Name $ " " $ Label);
	K = MenuScreenCustomizeKeys(S);
	if (K != None && K.lstKeys != None)
	{
		for (i = 0; i < K.lstKeys.GetNumRows(); i++)
		{
			Row = K.lstKeys.IndexToRowId(i);
			Log("DXRESETALL:   row " $ i $ " '" $ K.lstKeys.GetField(Row, 0) $ "' = '" $ K.lstKeys.GetField(Row, 1) $ "'");
		}
		return;
	}
	for (C = S.winClient.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		Choice = MenuUIChoice(C);
		if (Choice == None || MenuUIChoiceAction(Choice) != None)
			continue;
		Line = "DXRESETALL:   " $ Choice.Class.Name;
		E = MenuUIChoiceEnum(Choice);
		if (E != None)
			Line = Line $ " value=" $ E.currentValue $ " '" $ E.enumText[Clamp(E.currentValue, 0, 39)] $ "'";
		else if (MenuUIChoiceSlider(Choice) != None)
			Line = Line $ " value=" $ MenuUIChoiceSlider(Choice).GetValue();
		if (Choice.configSetting != "")
			Line = Line $ " get='" $ P.ConsoleCommand("get " $ Choice.configSetting) $ "'";
		Log(Line);
	}
}

function AnswerBoxes(DeusExRootWindow Root)
{
	local MenuUIMessageBoxWindow B;

	B = MenuUIMessageBoxWindow(Root.GetTopWindow());
	while (B != None)
	{
		Log("DXRESETALL: box '" $ B.winText.GetText() $ "' answered");
		if (B.btnOK != None)
			B.ButtonActivated(B.btnOK);
		else if (B.btnYes != None)
			B.ButtonActivated(B.btnYes);
		else
			Root.PopWindow();
		B = MenuUIMessageBoxWindow(Root.GetTopWindow());
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local MenuUIScreenWindow S;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Wait > 0)
	{
		Wait -= Delta;
		return;
	}
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	if (Phase == 0)
	{
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		Phase = 1;
		Wait = 1.0;
	}
	else if (Phase == 1)
	{
		if (ScreenClass(ScreenIndex) == None)
		{
			Log("DXRESETALL: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Root.InvokeMenuScreen(ScreenClass(ScreenIndex));
		Phase = 2;
		Wait = 1.0;
	}
	else if (Phase == 2)
	{
		S = MenuUIScreenWindow(Root.GetTopWindow());
		if (S == None)
		{
			Log("DXRESETALL: no screen " $ ScreenClass(ScreenIndex));
			ScreenIndex++;
			Phase = 1;
			return;
		}
		LogChoices(P, S, "as opened");
		S.ResetToDefaults();
		LogChoices(P, S, "after Reset to Defaults");
		if (SavedWithOK(ScreenIndex))
		{
			Log("DXRESETALL: OK on " $ S.Class.Name);
			S.SaveSettings();
			Root.PopWindow();
			Phase = 3;
		}
		else
		{
			Log("DXRESETALL: Cancel on " $ S.Class.Name);
			S.CancelScreen();
			ScreenIndex++;
			Phase = 1;
		}
		Wait = 1.0;
	}
	else if (Phase == 3)
	{
		AnswerBoxes(Root);
		Root.InvokeMenuScreen(ScreenClass(ScreenIndex));
		Phase = 4;
		Wait = 1.0;
	}
	else if (Phase == 4)
	{
		S = MenuUIScreenWindow(Root.GetTopWindow());
		if (S != None)
		{
			LogChoices(P, S, "reopened after OK");
			Root.PopWindow();
		}
		ScreenIndex++;
		Phase = 1;
		Wait = 1.0;
	}
}
