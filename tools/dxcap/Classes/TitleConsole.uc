//=============================================================================
// TitleConsole: the sizes two windows measure through GetGC outside a draw,
// logged with "DXTITLE:" (docs/DEVELOPMENT.md, scripted runs). On Liberty
// Island, 2 s in: the HUD's mission start text (HUDMissionStartTextDisplay)
// -- its place, size and preferred size; then the main menu opened, and a
// frame later its title bar (MenuUITitleWindow) the same, with its text;
// then the menu closed and an exit. What is compared between the engines:
// every line -- the original's GetGC gives a script its own window's fonts
// and settings, not the last drawn window's.
//=============================================================================
class TitleConsole extends Console;

var float RunTime;
var int Phase;

function Say(string S)
{
	Log("DXTITLE: " $ S);
}

function LogSize(string Label, Window W)
{
	local float PW, PH;

	if (W == None)
	{
		Say(Label $ ": none");
		return;
	}
	W.QueryPreferredSize(PW, PH);
	Say(Label $ ": at " $ W.x $ "," $ W.y $ " size " $ W.width $ "x" $ W.height $ ", preferred " $ PW $ "x" $ PH);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow root;
	local MenuUIWindow menu;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	RunTime += Delta;
	if (DXP == None)
		return;
	root = DeusExRootWindow(DXP.rootWindow);
	if (root == None)
		return;
	if (Phase == 0 && RunTime > 2.0)
	{
		if (root.hud != None)
			LogSize("the mission start text", root.hud.startDisplay);
		else
			Say("no HUD");
		root.InvokeMenu(class'MenuMain');
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 2.5)
	{
		menu = MenuUIWindow(root.GetTopWindow());
		if (menu == None)
			Say("no menu on top: " $ root.GetTopWindow());
		else
		{
			LogSize("the main menu", menu);
			LogSize("its title bar", menu.winTitle);
			if (menu.winTitle != None)
				Say("the title: " $ menu.winTitle.titleText);
		}
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 3.5)
	{
		root.ClearWindowStack();
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 4.0)
	{
		Say("done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
