//=============================================================================
// ResetConsole: the Display and Sound screens' Reset to Defaults, its
// SET of each choice's menu default as the screen's button sends it
// (MenuUIScreenWindow.ResetToDefaults, each choice's ResetToDefault), in
// both engines. From the menu map, the main menu shown at 3 s, Display
// Settings then Sound Settings are opened in turn: each choice is logged
// (its value, its text and the GET of its setting) as opened, then the
// screen's ResetToDefaults is called as its Reset to Defaults button calls
// it and the choices logged again, then the screen is closed with OK as the
// player saves it (a box asking to restart answered OK) and opened again
// and logged once more. Each line is logged with "DXRESET:" in front, then
// an exit.
//
// bugs.md ENG.29: World and Object Texture Detail's
// ResetToDefault sends "set ini:Engine.Engine.ViewportManager TextureDetail 0"
// (SkinDetail 0), its defaultValue an int; the original's enum import
// refuses a number and keeps the detail, DXEngine's client keeps the text.
//=============================================================================
class ResetConsole extends Console;

var float RunTime;
var float Wait;
var int Phase;
var int ScreenIndex;

function class<MenuUIScreenWindow> ScreenClass(int i)
{
	switch (i)
	{
	case 0: return class'MenuScreenDisplay';
	case 1: return class'MenuScreenSound';
	}
	return None;
}

function LogChoices(PlayerPawn P, MenuUIScreenWindow S, string Label)
{
	local Window C;
	local MenuUIChoice Choice;
	local MenuUIChoiceEnum E;
	local string Line;

	Log("DXRESET: " $ S.Class.Name $ " " $ Label);
	for (C = S.winClient.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		Choice = MenuUIChoice(C);
		if (Choice == None || MenuUIChoiceAction(Choice) != None)
			continue;
		Line = "DXRESET:   " $ Choice.Class.Name;
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
		Log("DXRESET: box '" $ B.winText.GetText() $ "' answered");
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
			Log("DXRESET: done, exiting");
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
			Log("DXRESET: no screen " $ ScreenClass(ScreenIndex));
			ScreenIndex++;
			Phase = 1;
			return;
		}
		LogChoices(P, S, "as opened");
		// The screen's Reset to Defaults button (AB_Reset)
		S.ResetToDefaults();
		LogChoices(P, S, "after Reset to Defaults");
		Log("DXRESET: OK on " $ S.Class.Name);
		S.SaveSettings();
		Root.PopWindow();
		Phase = 3;
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
