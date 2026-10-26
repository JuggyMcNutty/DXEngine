//=============================================================================
// ListConsole: a list window's drawing, the Load Game screen shot in both
// engines (docs/DEVELOPMENT.md, scripted runs), logged with "DXLIST:".
//
// On Liberty Island, 2 s in: two saves of the run's own, each by a
// SaveHelper on the actors' tick -- slot 8 named "a|cff0000b" (a colour
// code, which a list shows as typed) and slot 9 with a name 50 characters
// long, which runs past its column; 4 s later the Load Game screen is
// opened; 2 s later a marked shot; then the screen is closed, the two saves
// removed (DeleteGame 8 and 9), and the run exits. What is compared between
// the engines: the list -- the long name cut at its column's inner edge on
// one line, the code shown as typed, the fields' insets, and the focus
// row's outline.
//=============================================================================
class ListConsole extends Console;

var float RunTime;
var int Phase;
var int MarkShot;
var bool bMarking;

function SaveTo(DeusExPlayer DXP, int Slot, string Description)
{
	local SaveHelper H;

	H = DXP.Spawn(class'SaveHelper', DXP);
	if (H == None)
	{
		Log("DXLIST: no SaveHelper for slot " $ Slot);
		return;
	}
	H.Slot = Slot;
	H.Description = Description;
	Log("DXLIST: saving slot " $ Slot $ " as " $ Description);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow root;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP == None)
		return;

	if (Phase == 0 && RunTime > 2.0)
	{
		SaveTo(DXP, 8, "a|cff0000b");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.5)
	{
		SaveTo(DXP, 9, "A save whose name runs on and on, well past its column's edge");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 7.5)
	{
		root = DeusExRootWindow(DXP.rootWindow);
		if (root != None)
		{
			root.InvokeUIScreen(class'MenuScreenLoadGame', True);
			Log("DXLIST: Load Game screen opened");
		}
		else
			Log("DXLIST: no root window");
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 9.0)
	{
		MarkShot = 1;
		bMarking = true;
		Phase = 4;
	}
	else if (Phase == 4 && RunTime > 9.8)
	{
		Log("DXLIST: shot 1, the Load Game screen");
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		Phase = 5;
	}
	else if (Phase == 5 && RunTime > 10.5)
	{
		root = DeusExRootWindow(DXP.rootWindow);
		if (root != None)
			root.ClearWindowStack();
		P.ConsoleCommand("DeleteGame 8");
		P.ConsoleCommand("DeleteGame 9");
		Log("DXLIST: saves 8 and 9 removed");
		Phase = 6;
	}
	else if (Phase == 6 && RunTime > 12.0)
	{
		Log("DXLIST: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

// The mark for the original's screen grabber, as BorderConsole's.
event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}
