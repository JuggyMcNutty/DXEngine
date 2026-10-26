//=============================================================================
// FocusConsole: the keyboard focus when a window is made insensitive,
// logged with "DXFOCUS:" (docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island the player is given a lockpick, the inventory screen is
// opened and the lockpick selected on it, as BorderConsole does, so that a
// button has the focus. Then, the focus window logged after each: the
// focus window made insensitive; made sensitive again; its parent made
// insensitive; sensitive again. Then the screen is closed and the run
// exits. What is compared between the engines: every line -- the focus
// stays on the disabled button in both.
//=============================================================================
class FocusConsole extends Console;

var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var Inventory Pick;
var Window Focused, Parent;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

function string ClassOf(Window W)
{
	if (W == None)
		return "None";
	return string(W.Class.Name);
}

function Window FocusOf(PlayerPawn P)
{
	local DeusExRootWindow root;

	root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (root == None)
		return None;
	return root.GetFocusWindow();
}

function Say(PlayerPawn P, string When)
{
	local Window F;

	F = FocusOf(P);
	Log("DXFOCUS: " $ When $ ": focus " $ ClassOf(F) $ ", the button sensitive " $ Focused.IsSensitive() $ " (own " $ Focused.IsSensitive(False) $ "), its parent " $ ClassOf(Parent) $ " sensitive " $ Parent.IsSensitive(False));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow root;
	local PersonaScreenInventory screen;
	local string M;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	M = MapOf(P);
	if (M != CurrentMap)
	{
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 6.0)
		{
			Pick = P.Spawn(class'Lockpick',,, P.Location + vect(40, 0, 0));
			if (Pick != None)
			{
				Pick.GiveTo(DXP);
				Log("DXFOCUS: gave " $ Pick.Class.Name);
			}
			else
				Log("DXFOCUS: no lockpick spawned");
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		DXP.ShowInventoryWindow();
		Log("DXFOCUS: inventory screen opened, focus " $ ClassOf(FocusOf(P)));
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 1.0)
	{
		root = DeusExRootWindow(DXP.rootWindow);
		if (root != None)
			screen = PersonaScreenInventory(root.GetTopWindow());
		if (screen != None && Pick != None)
			screen.SelectInventoryItem(Pick);
		Focused = FocusOf(P);
		if (Focused == None)
		{
			Log("DXFOCUS: nothing has the focus after the selection, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Parent = Focused.GetParent();
		Say(P, "selected");
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 0.5)
	{
		Focused.SetSensitivity(False);
		Say(P, "the focused window made insensitive");
		StepTime = 0;
		Phase = 5;
	}
	else if (Phase == 5 && StepTime > 0.5)
	{
		Focused.SetSensitivity(True);
		Say(P, "made sensitive again");
		StepTime = 0;
		Phase = 6;
	}
	else if (Phase == 6 && StepTime > 0.5)
	{
		Parent.SetSensitivity(False);
		Say(P, "its parent made insensitive");
		StepTime = 0;
		Phase = 7;
	}
	else if (Phase == 7 && StepTime > 0.5)
	{
		Parent.SetSensitivity(True);
		Say(P, "its parent sensitive again");
		// The screen closed first: the original crashes exiting with it open
		root = DeusExRootWindow(DXP.rootWindow);
		if (root != None)
			root.ClearWindowStack();
		StepTime = 0;
		Phase = 8;
	}
	else if (Phase == 8 && StepTime > 0.5)
	{
		Log("DXFOCUS: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
