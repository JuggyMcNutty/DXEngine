//=============================================================================
// ColumnConsole: a list's columns fitted to their fields, the same in both
// engines (docs/DEVELOPMENT.md, scripted runs). From the menu map, the main
// menu shown, a list window of four columns is put on it, margins of 2 by 1:
// one field set in each, then the columns fitted (ResizeColumns(False)) and
// their widths logged ("DXCOLUMN:") -- "Cafe" and "Caf" with an e acute in
// FontMenuSmall, which has the e acute; "E" and two e acutes in
// FontMenuExtraLarge, which has none; "ABCabc" in FontSpinningDX, which has
// no small letters. Then the first field two lines, and the same; then the
// first column's font made FontMenuTitle, a larger one, and the last's
// None, the list's own, each width logged as it stands. Then an exit.
//=============================================================================
class ColumnConsole extends Console;

var float RunTime;
var int Step;

function LogWidths(ListWindow L, string Label)
{
	Log("DXCOLUMN: " $ Label $ ": widths " $ L.GetColumnWidth(0) $ ", " $ L.GetColumnWidth(1) $ ", "
		$ L.GetColumnWidth(2) $ ", " $ L.GetColumnWidth(3));
}

function Test(Window Holder)
{
	local ListWindow L;
	local int Row;

	L = ListWindow(Holder.NewChild(class'ListWindow'));
	L.SetSize(300, 100);
	L.SetNumColumns(4);
	L.SetFieldMargins(2, 1);
	L.SetColumnFont(0, Font'DeusExUI.FontMenuSmall');
	L.SetColumnFont(1, Font'DeusExUI.FontMenuSmall');
	L.SetColumnFont(2, Font'DeusExUI.FontMenuExtraLarge');
	L.SetColumnFont(3, Font'DeusExUI.FontSpinningDX');
	L.EnableAutoExpandColumns(True);
	Row = L.AddRow("");
	L.SetField(Row, 0, "Cafe");
	L.SetField(Row, 1, "Caf" $ Chr(233));
	L.SetField(Row, 2, "E" $ Chr(233) $ Chr(233));
	L.SetField(Row, 3, "ABCabc");
	L.ResizeColumns(False);
	LogWidths(L, "fitted");
	L.SetField(Row, 0, "Cafe" $ Chr(10) $ "Cafe Cafe");
	L.ResizeColumns(False);
	LogWidths(L, "the first field two lines, fitted");
	L.SetColumnFont(0, Font'DeusExUI.FontMenuTitle');
	LogWidths(L, "the first column's font FontMenuTitle");
	L.SetColumnFont(3, None);
	LogWidths(L, "the last column's font None");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		break;
	case 1:
		if (RunTime < 4.0)
			return;
		Test(Root.GetTopWindow());
		break;
	case 2:
		if (RunTime < 5.0)
			return;
		Log("DXCOLUMN: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
