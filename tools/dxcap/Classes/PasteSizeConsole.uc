//=============================================================================
// PasteSizeConsole (bugs.md EXT.43): what a paste much larger than an edit
// field's MaxSize costs. DXEngine's Paste takes the desktop's clipboard whole
// (NATIVES.md, the UI: by choice), puts every character through the field's
// script FilterChar -- a UStrProperty made and a script call each -- and only
// then cuts what passed to MaxSize (UEditWindow.cpp InsertText, ReplaceText).
// Logged "DXWID6:".
//
// From the menu map, the main menu shown (3 s), at 4 s:
//   a MenuUIEditWindow (its FilterChar a script function) of MaxSize 20 on the
//   main menu; a text of 10 * 2^17 = 1,310,720 characters made by doubling;
//   case A: Edit.InsertText(big) -- the same path in both engines;
//   case B: Player.CopyToClipboard(big), then Edit.SetText(""), Edit.Paste()
//   -- DXEngine's desktop clipboard (the original's own buffer is empty, so
//   nothing goes in there);
//   after each, the field's length, and the next tick's Delta (the stall).
// Then an exit at 8 s. What to look for: DXEngine's Delta after B (seconds,
// against a frame's), the process's memory during it (the GC's objects:
// `obj garbage` after), and the length 20 in both cases.
// tools/dxcap.sh dxe PasteSizeConsole DX.dx 12 ; the same with the original
// for case A's cost.
//=============================================================================
class PasteSizeConsole extends Console;

var float RunTime;
var int Step;
var MenuUIEditWindow Edit;
var string Big;
var bool bMeasure;
var string Pending;

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local int i;

	Super.Tick(Delta);
	if (bMeasure)
	{
		Log("DXWID6: " $ Pending $ ": the next tick's Delta " $ Delta $ ", the field's length " $ Len(Edit.GetText()));
		bMeasure = false;
	}
	if (Step == 99 || Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
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
		Edit = MenuUIEditWindow(Root.GetTopWindow().NewChild(class'MenuUIEditWindow'));
		Edit.SetPos(10, 10);
		Edit.SetSize(200, 20);
		Edit.SetMaxSize(20);
		Big = "abcdefghij";
		for (i = 0; i < 17; i++)
			Big = Big $ Big;
		Log("DXWID6: the text is " $ Len(Big) $ " characters; the tick's Delta before " $ Delta);
		Edit.InsertText(Big);
		Pending = "A, InsertText of it";
		bMeasure = true;
		break;
	case 2:
		if (RunTime < 5.5)
			return;
		Edit.SetText("");
		P.CopyToClipboard(Big);
		Log("DXWID6: copied to the clipboard; the tick's Delta before " $ Delta);
		Edit.Paste();
		Pending = "B, Paste of it";
		bMeasure = true;
		break;
	case 3:
		if (RunTime < 7.0)
			return;
		P.ConsoleCommand("obj garbage");
		Log("DXWID6: after obj garbage");
		break;
	case 4:
		if (RunTime < 8.0)
			return;
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
