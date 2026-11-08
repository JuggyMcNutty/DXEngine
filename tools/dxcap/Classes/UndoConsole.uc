//=============================================================================
// UndoConsole: what an edit window's Undo and Redo leave selected, and how its
// changed flag and its undo steps follow ClearTextChangedFlag. From the menu
// map, the main menu shown, an EditLogWindow is put on it with an edit window
// in it, and each step below is logged ("DXEDIT:") with the edit window's
// text, insertion point, selection and changed flag -- and every TextChanged
// it announces, with its bModified, as EditLogWindow logs it. Each check
// starts from a SetText, which clears the undo list. Then an exit.
//
// Expected (Extension.dll XEditWindow::Undo 0x1001e0a0, Redo 0x1001e170,
// ReplaceText 0x1001fe20, AddUndo 0x10020450, HasTextChanged 0x10013760,
// SetTextChangedFlag 0x1001ddc0, ClampUndo 0x10020290):
//  1 "Undo of a Backspace": the original 'abcdef', insertion point 3,
//    selected 2+1 (the 'c' put back selected); DXEngine selected 0+0. Then
//    InsertText('X', True): the original 'abXdef' (the selection replaced),
//    DXEngine 'abcXdef'.
//  2 "Redo of a typing": the original 'abcde', insertion point 5, selected
//    3+2; DXEngine selected 0+0.
//  3 "Undo back to the clean mark": the original changed False and its
//    TextChanged modified False; DXEngine True and True. "Undo after a
//    ClearTextChangedFlag": the original 'abcd', changed False (the 'e'
//    typed after the mark its own step); DXEngine 'abc', changed True (the
//    'e' joined to the 'd').
//  4 "SetMaxUndos(0)": the original 'abcd' (no step kept); DXEngine 'abc'
//    (0 taken as no limit). No stock script calls SetMaxUndos.
//  5 A CapLargeTextGCWindow (centred, no word wrap) put on the menu beside
//    the edit window logs, at its first draw, its GC's alignments and word
//    wrap: the original 0, 0, True; DXEngine 1, 1, False.
//=============================================================================
class UndoConsole extends Console;

var float RunTime;
var int Step;
var EditLogWindow Holder;
var EditWindow Edit;
var CapLargeTextGCWindow Large;

function LogEdit(string Label)
{
	local int Start, Count;

	Edit.GetSelectedArea(Start, Count);
	Log("DXEDIT: " $ Label $ ": text '" $ Edit.GetText() $ "', insertion point " $ Edit.GetInsertionPoint()
		$ ", selected " $ Start $ "+" $ Count $ ", changed " $ Edit.HasTextChanged());
}

function TestDeletion()
{
	Log("DXEDIT: -- 1 a Backspace undone");
	Edit.SetText("abcdef");
	Edit.SetInsertionPoint(3);
	LogEdit("SetText('abcdef'), SetInsertionPoint(3)");
	Edit.DeleteChar(True, True);
	LogEdit("DeleteChar(True, True)");
	Edit.Undo();
	LogEdit("Undo of a Backspace");
	Edit.InsertText("X", True);
	LogEdit("InsertText('X', True)");
	Edit.Undo();
	LogEdit("Undo");
	Edit.Redo();
	LogEdit("Redo");
}

function TestTyping()
{
	Log("DXEDIT: -- 2 a typing undone and redone");
	Edit.SetText("abc");
	Edit.SetInsertionPoint(3);
	Edit.InsertText("de", True);
	LogEdit("SetText('abc'), SetInsertionPoint(3), InsertText('de', True)");
	Edit.Undo();
	LogEdit("Undo of a typing");
	Edit.Redo();
	LogEdit("Redo of a typing");
}

function TestCleanMark()
{
	Log("DXEDIT: -- 3 the clean mark");
	Edit.SetText("abc");
	Edit.ClearTextChangedFlag();
	Edit.SetInsertionPoint(3);
	LogEdit("SetText('abc'), ClearTextChangedFlag, SetInsertionPoint(3)");
	Edit.InsertText("d", True);
	LogEdit("InsertText('d', True)");
	Edit.Undo();
	LogEdit("Undo back to the clean mark");
	Edit.Redo();
	Edit.SetInsertionPoint(4);
	LogEdit("Redo, SetInsertionPoint(4)");
	Edit.ClearTextChangedFlag();
	LogEdit("ClearTextChangedFlag");
	Edit.InsertText("e", True);
	LogEdit("InsertText('e', True)");
	Edit.Undo();
	LogEdit("Undo after a ClearTextChangedFlag");
	Edit.Undo();
	LogEdit("Undo again");
}

function TestMaxUndos()
{
	Log("DXEDIT: -- 4 SetMaxUndos(0)");
	Edit.SetText("abc");
	Edit.SetMaxUndos(0);
	Edit.SetInsertionPoint(3);
	Edit.InsertText("d", True);
	LogEdit("SetMaxUndos(0), InsertText('d', True)");
	Edit.Undo();
	LogEdit("Undo with SetMaxUndos(0)");
	Edit.SetMaxUndos(64);
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
		Holder = EditLogWindow(Root.GetTopWindow().NewChild(class'EditLogWindow'));
		Holder.SetPos(10, 10);
		Holder.SetSize(300, 200);
		Edit = EditWindow(Holder.NewChild(class'EditWindow'));
		Edit.SetSize(200, 20);
		TestDeletion();
		TestTyping();
		TestCleanMark();
		TestMaxUndos();
		Large = CapLargeTextGCWindow(Root.GetTopWindow().NewChild(class'CapLargeTextGCWindow'));
		Large.SetPos(10, 240);
		Large.SetSize(200, 40);
		Large.SetText("Liberty Island");
		break;
	case 2:
		if (RunTime < 5.0)
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
