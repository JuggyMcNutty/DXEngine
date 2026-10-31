//=============================================================================
// FocusConsole: the keyboard focus when a window is made insensitive,
// logged with "DXFOCUS:" (docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island the player is given a lockpick, the inventory screen is
// opened and the lockpick selected on it, as BorderConsole does, so that a
// button has the focus. Then, the focus window logged after each: the
// focus window made insensitive; made sensitive again; its parent made
// insensitive; sensitive again. Then the screen is closed.
//
// Then a window clipped out of view: a modal of its own (focus by clicks
// alone, at 20, 20, 400 by 400) holds button A1 at 10, 60; a clip window at
// 10, 100, 200 by 60, its child 200 by 300 holding buttons B1, B2 and B3 at
// 10, 0, 100 and 200; and a tab group at 10, 200 holding D1 -- each button
// 50 by 20. The clip window is scrolled 100 down, B1 above its view, B2 in
// it, and the first focus logged. Then, the clip window scrolled again
// before each and the focus put on the window named: from D1 back a tab
// group, B1 above the view; from A1 on a tab group; and, with the clip
// scrolled 30 down so that none of its buttons shows, from A1 on and back.
// After each, the focus and the clip window's row. Last, with the clip at
// 30 again, each button's clip rectangle (winClipRect) from the modal's
// corner. Then the modal is destroyed and the run exits. What is compared
// between the engines: every line -- the focus stays on the disabled
// button in both; each tab group goes in the order of the first of its
// windows that shows, a group none of whose windows shows at 0, 0.
//=============================================================================
class FocusConsole extends Console;

var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var Inventory Pick;
var Window Focused, Parent;
var ModalWindow Modal;
var ClipWindow Clip;
var TabGroupWindow GroupD;
var ButtonWindow A1, B1, B2, B3, D1;

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

// The clip test's windows by their names, any other by its class.
function string NameOf(Window W)
{
	if (W == A1) return "A1";
	if (W == B1) return "B1";
	if (W == B2) return "B2";
	if (W == B3) return "B3";
	if (W == D1) return "D1";
	return ClassOf(W);
}

function ButtonWindow AddButton(Window Holder, float X, float Y)
{
	local ButtonWindow B;

	B = ButtonWindow(Holder.NewChild(class'ButtonWindow'));
	B.SetPos(X, Y);
	B.SetSize(50, 20);
	return B;
}

function SayClip(PlayerPawn P, string When)
{
	local int Col, Row;

	Clip.GetChildPosition(Col, Row);
	Log("DXFOCUS: clip: " $ When $ ": focus " $ NameOf(FocusOf(P)) $ ", the clip at row " $ Row);
}

// A window's clip rectangle, its origin from the modal's.
function SayRect(Window W)
{
	Log("DXFOCUS: clip: " $ NameOf(W) $ " at " $ int(W.winClipRect.originX - Modal.winClipRect.originX) $ ","
		$ int(W.winClipRect.originY - Modal.winClipRect.originY) $ ", in view from " $ int(W.winClipRect.clipX) $ ","
		$ int(W.winClipRect.clipY) $ " " $ int(W.winClipRect.clipWidth) $ " by " $ int(W.winClipRect.clipHeight));
}

// The focus put on a window and the clip window scrolled to a row, for the
// next step's move once both engines have laid it out.
function SetUp(Window Start, int Row)
{
	Modal.SetFocusWindow(Start);
	Clip.SetChildPosition(0, Row);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow root;
	local PersonaScreenInventory screen;
	local string M;
	local Window Holder;

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
		root = DeusExRootWindow(DXP.rootWindow);
		if (root == None)
		{
			Log("DXFOCUS: no root window for the clip test, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Modal = ModalWindow(root.NewChild(class'ModalWindow'));
		Modal.SetMouseFocusMode(MFOCUS_Click);
		Modal.SetPos(20, 20);
		Modal.SetSize(400, 400);
		A1 = AddButton(Modal, 10, 60);
		Clip = ClipWindow(Modal.NewChild(class'ClipWindow'));
		Clip.SetPos(10, 100);
		Clip.SetSize(200, 60);
		Holder = Clip.NewChild(class'Window');
		Holder.SetSize(200, 300);
		B1 = AddButton(Holder, 10, 0);
		B2 = AddButton(Holder, 10, 100);
		B3 = AddButton(Holder, 10, 200);
		GroupD = TabGroupWindow(Modal.NewChild(class'TabGroupWindow'));
		GroupD.SetPos(10, 200);
		GroupD.SetSize(100, 40);
		D1 = AddButton(GroupD, 0, 0);
		Clip.SetChildPosition(0, 100);
		Log("DXFOCUS: clip: the modal made, the clip scrolled 100 down");
		StepTime = 0;
		Phase = 10;
	}
	else if (Phase == 10 && StepTime > 0.5)
	{
		SayClip(P, "the first focus, B1 above the view");
		SetUp(D1, 100);
		StepTime = 0;
		Phase = 11;
	}
	else if (Phase == 11 && StepTime > 0.5)
	{
		Modal.MoveTabGroupPrev();
		SayClip(P, "from D1 back a tab group, B1 above the view");
		SetUp(A1, 100);
		StepTime = 0;
		Phase = 12;
	}
	else if (Phase == 12 && StepTime > 0.5)
	{
		Modal.MoveTabGroupNext();
		SayClip(P, "from A1 on a tab group, B1 above the view");
		SetUp(A1, 30);
		StepTime = 0;
		Phase = 13;
	}
	else if (Phase == 13 && StepTime > 0.5)
	{
		Modal.MoveTabGroupNext();
		SayClip(P, "from A1 on a tab group, none of the clip's buttons in view");
		SetUp(A1, 30);
		StepTime = 0;
		Phase = 14;
	}
	else if (Phase == 14 && StepTime > 0.5)
	{
		Modal.MoveTabGroupPrev();
		SayClip(P, "from A1 back a tab group, none of the clip's buttons in view");
		Clip.SetChildPosition(0, 30);
		StepTime = 0;
		Phase = 15;
	}
	else if (Phase == 15 && StepTime > 0.5)
	{
		SayClip(P, "the clip scrolled 30 down again");
		SayRect(A1);
		SayRect(B1);
		SayRect(B2);
		SayRect(B3);
		SayRect(D1);
		Modal.Destroy();
		Modal = None;
		StepTime = 0;
		Phase = 16;
	}
	else if (Phase == 16 && StepTime > 0.5)
	{
		Log("DXFOCUS: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
