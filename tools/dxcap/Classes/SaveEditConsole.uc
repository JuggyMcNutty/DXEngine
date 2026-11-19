//=============================================================================
// SaveEditConsole (bugs.md EXT.41): a list window's lineSize before its first
// draw, and the Save Game screen's name field, which MenuScreenSaveGame's
// MoveEditControl places at the new row's index times lstGames.lineSize,
// from its InitWindow -- before the list has drawn. Logged "DXWID6:".
//
// On Liberty Island (DX.dx is fine too, but a level that saves):
//   2 s  a bare ListWindow on the root, one column in FontMenuSmall, three
//        rows: its lineSize logged at once (no draw yet), with each row's
//        index after AutoSort on a float column (the sort's direction);
//        lineSize logged again a second later, after draws;
//   3 s  save to slot 8; 5.5 s save to slot 9 (a different second, so the
//        dates differ);
//   8 s  the Save Game screen opened as the main menu's Save Game opens it
//        (InvokeMenuScreen): the same tick, lstGames' rows in order (name
//        field, date number, directory index), lstGames.lineSize, the new
//        save's row index and clipName's y and visibility;
//   9 s  the same again (after draws), and a marked shot (shot 1);
//  10 s  the screen closed, saves 8 and 9 deleted, exit.
// What to compare: "lineSize at once" (DXEngine 0 by reading; the original
// the row height if its ComputeRowSize stores it, as the stock script
// assumes), "clipName y" at 8 s against the new row's index times the row
// height plus 1, and shot 1: the name field over the new row or over row 0.
// tools/dxcap.sh dxe SaveEditConsole 01_NYC_UNATCOIsland 14 ; the same with
// the original.
//=============================================================================
class SaveEditConsole extends Console;

var float RunTime;
var int Phase;
var ListWindow Bare;
var int MarkShot;
var bool bMarking;

function SaveTo(DeusExPlayer DXP, int Slot, string Description)
{
	local SaveHelper H;

	H = DXP.Spawn(class'SaveHelper', DXP);
	if (H == None)
	{
		Log("DXWID6: no SaveHelper for slot " $ Slot);
		return;
	}
	H.Slot = Slot;
	H.Description = Description;
	Log("DXWID6: saving slot " $ Slot $ " as " $ Description);
}

function MakeBare(DeusExRootWindow Root)
{
	local int a, b, c;

	Bare = ListWindow(Root.NewChild(class'ListWindow'));
	Bare.SetPos(10, 10);
	Bare.SetNumColumns(2);
	Bare.SetColumnFont(0, Font'DeusExUI.FontMenuSmall');
	Bare.SetColumnType(1, COLTYPE_Float);
	Bare.SetSortColumn(1, False);
	a = Bare.AddRow("middle;5");
	b = Bare.AddRow("high;9");
	c = Bare.AddRow("low;1");
	Log("DXWID6: bare list, lineSize at once " $ Bare.lineSize);
	Bare.EnableAutoSort(True);
	Log("DXWID6: bare list sorted ascending? rows at index: low " $ Bare.RowIdToIndex(c)
		$ ", middle " $ Bare.RowIdToIndex(a) $ ", high " $ Bare.RowIdToIndex(b));
}

function SayScreen(string Label, DeusExRootWindow Root)
{
	local MenuScreenSaveGame S;
	local int i, n, id;

	S = MenuScreenSaveGame(Root.GetTopWindow());
	if (S == None)
	{
		Log("DXWID6: " $ Label $ ": no Save Game screen on top");
		return;
	}
	n = S.lstGames.GetNumRows();
	Log("DXWID6: " $ Label $ ": " $ n $ " rows, lineSize " $ S.lstGames.lineSize
		$ ", new save's row index " $ S.lstGames.RowIdToIndex(S.newSaveRowId)
		$ ", clipName y " $ S.clipName.y $ ", shown " $ S.clipName.IsVisible(False));
	for (i = 0; i < n; i++)
	{
		id = S.lstGames.IndexToRowId(i);
		Log("DXWID6: " $ Label $ ":   row " $ i $ ": '" $ S.lstGames.GetField(id, 0) $ "' date "
			$ S.lstGames.GetField(id, 2) $ " index " $ S.lstGames.GetField(id, 4));
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;
	P.ReducedDamageType = 'All';
	RunTime += Delta;
	Root = DeusExRootWindow(DXP.rootWindow);
	if (Root == None)
		return;

	if (Phase == 0 && RunTime > 2.0)
	{
		MakeBare(Root);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXWID6: bare list, lineSize after a second of draws " $ Bare.lineSize);
		Bare.Destroy();
		Bare = None;
		SaveTo(DXP, 8, "SaveEdit first");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 5.5)
	{
		SaveTo(DXP, 9, "SaveEdit second");
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 8.0)
	{
		Root.InvokeMenuScreen(class'MenuScreenSaveGame');
		SayScreen("opened, same tick", Root);
		Phase = 4;
	}
	else if (Phase == 4 && RunTime > 9.0)
	{
		SayScreen("a second later", Root);
		MarkShot = 1;
		bMarking = true;
		Phase = 5;
	}
	else if (Phase == 5 && RunTime > 9.6)
	{
		Log("DXWID6: shot 1, the Save Game screen");
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		Phase = 6;
	}
	else if (Phase == 6 && RunTime > 10.5)
	{
		Root.ClearWindowStack();
		P.ConsoleCommand("DeleteGame 8");
		P.ConsoleCommand("DeleteGame 9");
		Log("DXWID6: saves 8 and 9 removed");
		Phase = 7;
	}
	else if (Phase == 7 && RunTime > 12.0)
	{
		Log("DXWID6: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

// The mark for the original's screen grabber, as ListConsole's.
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
