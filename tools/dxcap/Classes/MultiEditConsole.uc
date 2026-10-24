//=============================================================================
// MultiEditConsole: what moving the insertion point does in an edit window of
// several lines, as a note's, and how it draws a selection over rows. From
// the menu map, the main menu shown, an EditLogWindow is put on it with an
// edit window in it: several lines, FontMenuSmall, margins 10 and 5 as the
// notes have them, 200 by 150, with a text of rows the window wraps, a line
// break, an empty row, a word wider than a row and spaces at a wrap. A tick
// later, once both engines have laid it out, each of the twelve moves is
// made from fourteen places, then chains of them -- down and up keeping the
// column, a drag down and across, every word right and left, pages down and
// up -- then a selection over rows and a text put in selected, then the same
// moves in a single-line edit window; each logged ("DXROWS:") with the
// insertion point and the selection. Then the window is given the focus, a
// selection over three rows and an insertion point that stays on, and two
// shots are taken 2 s apart, each marked for the original's grabber; then
// an exit. With Keys (DXCAP_CONFIG "Keys=True", DXEngine's alone), the
// window is given the focus instead, its insertion point at 7, and each
// change of its insertion point or selection is logged as the keys of
// tools/dxcap/timelines/note-keys.txt reach it, until an exit at 30 s.
//=============================================================================
class MultiEditConsole extends Console;

var config bool Keys;
var int LastPos, LastStart, LastCount;

var float RunTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var EditLogWindow Holder;
var EditWindow Edit;
var EditWindow Single;

// The mark for the original's screen grabber, as TileConsole's.
var int MarkShot;
var bool bMarking;

function string MoveName(int M)
{
	switch (M)
	{
	case 0: return "Up";
	case 1: return "Down";
	case 2: return "Left";
	case 3: return "Right";
	case 4: return "WordLeft";
	case 5: return "WordRight";
	case 6: return "StartOfLine";
	case 7: return "EndOfLine";
	case 8: return "PageUp";
	case 9: return "PageDown";
	case 10: return "Home";
	case 11: return "End";
	}
	return "?";
}

function Move(EditWindow E, int M, bool bDrag)
{
	switch (M)
	{
	case 0: E.MoveInsertionPoint(MOVEINSERT_Up, bDrag); break;
	case 1: E.MoveInsertionPoint(MOVEINSERT_Down, bDrag); break;
	case 2: E.MoveInsertionPoint(MOVEINSERT_Left, bDrag); break;
	case 3: E.MoveInsertionPoint(MOVEINSERT_Right, bDrag); break;
	case 4: E.MoveInsertionPoint(MOVEINSERT_WordLeft, bDrag); break;
	case 5: E.MoveInsertionPoint(MOVEINSERT_WordRight, bDrag); break;
	case 6: E.MoveInsertionPoint(MOVEINSERT_StartOfLine, bDrag); break;
	case 7: E.MoveInsertionPoint(MOVEINSERT_EndOfLine, bDrag); break;
	case 8: E.MoveInsertionPoint(MOVEINSERT_PageUp, bDrag); break;
	case 9: E.MoveInsertionPoint(MOVEINSERT_PageDown, bDrag); break;
	case 10: E.MoveInsertionPoint(MOVEINSERT_Home, bDrag); break;
	case 11: E.MoveInsertionPoint(MOVEINSERT_End, bDrag); break;
	}
}

function LogAt(EditWindow E, string Label)
{
	local int Start, Count;

	E.GetSelectedArea(Start, Count);
	Log("DXROWS: " $ Label $ ": insertion point " $ E.GetInsertionPoint() $ ", selected " $ Start $ "+" $ Count);
}

function MoveAndLog(EditWindow E, int M, bool bDrag, string Label)
{
	local string Suffix;

	Move(E, M, bDrag);
	if (bDrag)
		Suffix = " drag";
	LogAt(E, Label $ " " $ MoveName(M) $ Suffix);
}

function string NoteText()
{
	return "The quick brown fox jumps over the lazy dog at the pier of Liberty Island, a wide bay."
		$ Chr(10) $ Chr(10) $ "Averyveryverylongwordthatwillnotfitinonerow at all." $ Chr(10) $ "End  of  note";
}

function MakeWindows(DeusExRootWindow Root)
{
	local color Selection;

	Selection.R = 0;
	Selection.G = 96;
	Selection.B = 192;
	Holder = EditLogWindow(Root.GetTopWindow().NewChild(class'EditLogWindow'));
	Holder.SetPos(10, 30);
	Holder.SetSize(400, 400);

	Edit = EditWindow(Holder.NewChild(class'EditWindow'));
	Edit.EnableSingleLineEditing(False);
	Edit.SetFont(Font'DeusExUI.FontMenuSmall');
	Edit.SetTextMargins(10, 5);
	Edit.SetPos(0, 0);
	Edit.SetSize(200, 150);
	Edit.SetBackground(Texture'Solid');
	Edit.SetBackgroundStyle(DSTY_Normal);
	Edit.SetTileColorRGB(32, 32, 32);
	Edit.SetTextColorRGB(255, 255, 255);
	Edit.SetInsertionPointTexture(Texture'Solid');
	Edit.SetInsertionPointBlinkRate(1000, 1);
	Edit.SetSelectedAreaTexture(Texture'Solid', Selection);
	Edit.SetText(NoteText());

	Single = EditWindow(Holder.NewChild(class'EditWindow'));
	Single.EnableSingleLineEditing(True);
	Single.SetFont(Font'DeusExUI.FontMenuSmall');
	Single.SetPos(0, 200);
	Single.SetSize(120, 20);
	Single.SetText("one two three four five six seven eight nine ten eleven twelve");
}

function TestMoves()
{
	local int i, M, TextLen;
	local int Places[14];

	TextLen = Len(NoteText());
	LogAt(Edit, "made, " $ TextLen $ " characters");
	Places[0] = 0;
	Places[1] = 4;
	Places[2] = 10;
	Places[3] = 25;
	Places[4] = 40;
	Places[5] = 50;
	Places[6] = 70;
	Places[7] = 85;
	Places[8] = 86;
	Places[9] = 87;
	Places[10] = 100;
	Places[11] = 120;
	Places[12] = TextLen - 3;
	Places[13] = TextLen;
	for (i = 0; i < 14; i++)
	{
		for (M = 0; M < 12; M++)
		{
			Edit.SetInsertionPoint(Places[i]);
			MoveAndLog(Edit, M, False, "from " $ Places[i]);
		}
	}

	Edit.SetInsertionPoint(7);
	for (i = 0; i < 7; i++)
		MoveAndLog(Edit, 1, False, "chain from 7");
	for (i = 0; i < 7; i++)
		MoveAndLog(Edit, 0, False, "chain back");

	Edit.SetInsertionPoint(7);
	MoveAndLog(Edit, 1, True, "drag from 7");
	MoveAndLog(Edit, 1, True, "drag on");
	MoveAndLog(Edit, 3, True, "drag on");
	MoveAndLog(Edit, 2, False, "then");

	Edit.SetInsertionPoint(0);
	for (i = 0; i < 24; i++)
		MoveAndLog(Edit, 5, False, "words from 0");
	for (i = 0; i < 24; i++)
		MoveAndLog(Edit, 4, False, "words back");

	Edit.SetInsertionPoint(0);
	for (i = 0; i < 3; i++)
		MoveAndLog(Edit, 9, False, "pages from 0");
	for (i = 0; i < 3; i++)
		MoveAndLog(Edit, 8, False, "pages back");

	Edit.SetSelectedArea(5, 60);
	LogAt(Edit, "SetSelectedArea(5, 60)");
	Edit.InsertText("ABC", True, True);
	LogAt(Edit, "InsertText('ABC') selected");
	Log("DXROWS: text now '" $ Edit.GetText() $ "'");
	Edit.SetText(NoteText());
	LogAt(Edit, "SetText again");

	for (M = 0; M < 12; M++)
	{
		Single.SetInsertionPoint(10);
		MoveAndLog(Single, M, False, "single from 10");
	}
}

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

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	StepTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	switch (Phase)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		break;
	case 1:
		if (RunTime < 4.0)
			return;
		MakeWindows(Root);
		break;
	case 2:
		if (Keys)
		{
			Root.SetFocusWindow(Edit);
			Edit.SetInsertionPoint(7);
			LogAt(Edit, "keys from");
			Phase = 10;
			return;
		}
		TestMoves();
		Edit.SetSelectedArea(20, 80);
		Root.SetFocusWindow(Edit);
		LogAt(Edit, "for the shots");
		StepTime = 0;
		break;
	case 3:
		// A shot every 2 s, the mark shown for the 0.6 s before each.
		if (StepTime > 2.0 && ShotsTaken < 2)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 1.4;
			}
			else
			{
				Log("DXROWS: shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
			}
			return;
		}
		if (ShotsTaken < 2 || StepTime < 2.0)
			return;
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
		return;
	case 10:
		WatchKeys(P);
		return;
	default:
		return;
	}
	Phase++;
}

// Keys mode: a line for each change the timeline's keys make.
function WatchKeys(PlayerPawn P)
{
	local int Start, Count;

	Edit.GetSelectedArea(Start, Count);
	if (Edit.GetInsertionPoint() != LastPos || Start != LastStart || Count != LastCount)
	{
		LastPos = Edit.GetInsertionPoint();
		LastStart = Start;
		LastCount = Count;
		LogAt(Edit, "a key");
	}
	if (RunTime > 30.0)
	{
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}
