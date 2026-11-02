//=============================================================================
// ClipDrawConsole: what a window's clip rectangle holds back, the same in
// both engines (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, the
// UI), logged with "DXCLIPDRAW:" (bugs.md EXT.20).
//
// On the menu map, 3 s in, two holders on the root, each 300 by 400. The
// first, H1 at 20, 20, a DrawCountWindow whose draws are the frames, holds:
// a clip window at 10, 10, 200 by 60, its child 200 by 300 holding three
// DrawCountWindows 50 by 20, W1 at 10, 0, W2 at 10, 100 and W3 at 10, 200,
// each holding a DrawCountWindow 10 by 10 at 5, 5 (K1, K2, K3); Flat, 50
// by 0 at 10, 80; Outside, 50 by 20 at 400, 80, past H1's right edge; and
// a clip window at 10, 120, 200 by 60, its child 200 by 300 holding an edit
// field of the same size, 30 lines of FontMenuSmall. The second, H2 at 330,
// 20, holds four lists of one column, 40 rows of FontMenuSmall: L1 straight
// in a clip window at 10, 10, 200 by 60, as a scroll area holds one; L2 200
// by 300 in a window 200 by 300 in a clip window at 10, 80, 200 by 60; L3
// 200 by 60 straight on H2 at 10, 150, as the New Game screen's skill list
// sits; L4 straight in a clip window at 10, 300, 200 by 200, which H2 cuts
// to 100.
//
// Then the first clip window is scrolled to rows 0, 100, 110, 116, 30 and
// 200 in turn: half a second after each scroll every count is cleared, and
// a second later each window's DrawWindow and PostDrawWindow events, and its
// child's, are logged against H1's ("every frame", "none", or how many of
// how many), with the window's clip rectangle from H1's corner. At row 110
// W2 and K2 show in part; at 116 W2 shows 4 units and K2 none; at 30 none
// of the three shows. Then each list's row size, clip rectangle's height,
// parent's height and page size (GetPageSize), and the row a Page Down from
// the first row reaches; then the edit field's insertion point after Home
// and a Page Down. Then an exit.
//
// What is compared between the engines: every line. The original sends no
// event to a window none of which shows, nor to any window in it -- Flat
// and Outside none at any row, each W and its K none out of view, K2 none
// at 116 -- and pages a list by the height of it its clip rectangle holds
// (L2's 60, L3's 60, L4's 100, not its parent's 300, 400 and 200).
//=============================================================================
class ClipDrawConsole extends Console;

var float RunTime;
var float StepTime;
var int Phase;
var int Step;
var DrawCountWindow H1, W1, W2, W3, K1, K2, K3, Flat, Outside;
var Window H2;
var ClipWindow Clip, EditClip;
var EditWindow Edit;
var ListWindow L1, L2, L3, L4;
var int Rows[6];

function DrawCountWindow AddCounter(Window Holder, float X, float Y, float W, float H)
{
	local DrawCountWindow C;

	C = DrawCountWindow(Holder.NewChild(class'DrawCountWindow'));
	C.SetPos(X, Y);
	C.SetSize(W, H);
	return C;
}

// A window counted, with one 10 by 10 at 5, 5 in it.
function DrawCountWindow AddPair(Window Holder, float Y, out DrawCountWindow Child)
{
	local DrawCountWindow C;

	C = AddCounter(Holder, 10, Y, 50, 20);
	Child = AddCounter(C, 5, 5, 10, 10);
	return C;
}

function ListWindow AddList(Window Holder)
{
	local ListWindow L;
	local int i;

	L = ListWindow(Holder.NewChild(class'ListWindow'));
	L.SetNumColumns(1);
	L.SetColumnWidth(0, 200);
	L.SetColumnFont(0, Font'DeusExUI.FontMenuSmall');
	for (i = 1; i <= 40; i++)
		L.AddRow("Row " $ i);
	return L;
}

function Make(DeusExRootWindow Root)
{
	local Window Inner;
	local string Lines;
	local int i;

	H1 = DrawCountWindow(Root.NewChild(class'DrawCountWindow'));
	H1.SetPos(20, 20);
	H1.SetSize(300, 400);
	Clip = ClipWindow(H1.NewChild(class'ClipWindow'));
	Clip.SetPos(10, 10);
	Clip.SetSize(200, 60);
	Inner = Clip.NewChild(class'Window');
	Inner.SetSize(200, 300);
	W1 = AddPair(Inner, 0, K1);
	W2 = AddPair(Inner, 100, K2);
	W3 = AddPair(Inner, 200, K3);
	Flat = AddCounter(H1, 10, 80, 50, 0);
	Outside = AddCounter(H1, 400, 80, 50, 20);

	EditClip = ClipWindow(H1.NewChild(class'ClipWindow'));
	EditClip.SetPos(10, 120);
	EditClip.SetSize(200, 60);
	Inner = EditClip.NewChild(class'Window');
	Inner.SetSize(200, 300);
	Edit = EditWindow(Inner.NewChild(class'EditWindow'));
	Edit.EnableSingleLineEditing(False);
	Edit.SetFont(Font'DeusExUI.FontMenuSmall');
	Edit.SetPos(0, 0);
	Edit.SetSize(200, 300);
	for (i = 1; i <= 30; i++)
	{
		if (i < 10)
			Lines = Lines $ "Line 0" $ i;
		else
			Lines = Lines $ "Line " $ i;
		if (i < 30)
			Lines = Lines $ Chr(10);
	}
	Edit.SetText(Lines);

	H2 = Root.NewChild(class'Window');
	H2.SetPos(330, 20);
	H2.SetSize(300, 400);
	L1 = AddList(AddClip(H2, 10, 10, 200, 60));
	Inner = AddClip(H2, 10, 80, 200, 60).NewChild(class'Window');
	Inner.SetSize(200, 300);
	L2 = AddList(Inner);
	L2.SetPos(0, 0);
	L2.SetSize(200, 300);
	L3 = AddList(H2);
	L3.SetPos(10, 150);
	L3.SetSize(200, 60);
	L4 = AddList(AddClip(H2, 10, 300, 200, 200));
}

function Window AddClip(Window Holder, float X, float Y, float W, float H)
{
	local ClipWindow C;

	C = ClipWindow(Holder.NewChild(class'ClipWindow'));
	C.SetPos(X, Y);
	C.SetSize(W, H);
	return C;
}

function int ClipRow()
{
	local int Col, Row;

	Clip.GetChildPosition(Col, Row);
	return Row;
}

function string Times(int N, int Frames)
{
	if (N == 0)
		return "none";
	if (N == Frames)
		return "every frame";
	return N $ " of " $ Frames;
}

// A window's clip rectangle, its origin from H1's.
function string RectOf(Window W)
{
	return "at " $ int(W.winClipRect.originX - H1.winClipRect.originX) $ "," $ int(W.winClipRect.originY - H1.winClipRect.originY)
		$ ", in view from " $ int(W.winClipRect.clipX) $ "," $ int(W.winClipRect.clipY) $ " "
		$ int(W.winClipRect.clipWidth) $ " by " $ int(W.winClipRect.clipHeight);
}

function SayCounts(string Label, DrawCountWindow W, DrawCountWindow Child)
{
	local string S;

	S = "DXCLIPDRAW: row " $ Rows[Step] $ ": " $ Label $ " DrawWindow " $ Times(W.Draws, H1.Draws)
		$ ", PostDrawWindow " $ Times(W.PostDraws, H1.Draws);
	if (Child != None)
		S = S $ "; its child " $ Times(Child.Draws, H1.Draws) $ ", " $ Times(Child.PostDraws, H1.Draws);
	Log(S $ "; " $ RectOf(W));
}

function ClearAll()
{
	H1.ClearCounts();
	W1.ClearCounts();
	W2.ClearCounts();
	W3.ClearCounts();
	K1.ClearCounts();
	K2.ClearCounts();
	K3.ClearCounts();
	Flat.ClearCounts();
	Outside.ClearCounts();
}

function SayList(string Label, ListWindow L)
{
	local int Page;

	Page = L.GetPageSize();
	L.SetRow(L.IndexToRowId(0), True, True, False);
	L.MoveRow(MOVELIST_PageDown, True, True, False);
	Log("DXCLIPDRAW: " $ Label $ ": row size " $ L.lineSize $ ", its clip rectangle's height " $ int(L.winClipRect.clipHeight)
		$ ", its parent's height " $ int(L.GetParent().height) $ ", page " $ Page $ ", Page Down from the first row to row "
		$ L.RowIdToIndex(L.GetFocusRow()));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	StepTime += Delta;

	if (Phase == 0 && RunTime > 3.0)
	{
		Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		if (Root == None)
			return;
		Rows[0] = 0;
		Rows[1] = 100;
		Rows[2] = 110;
		Rows[3] = 116;
		Rows[4] = 30;
		Rows[5] = 200;
		Make(Root);
		Log("DXCLIPDRAW: made");
		Step = 0;
		Clip.SetChildPosition(0, Rows[Step]);
		StepTime = 0;
		Phase = 1;
	}
	else if (Phase == 1 && StepTime > 0.5)
	{
		ClearAll();
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		Log("DXCLIPDRAW: row " $ Rows[Step] $ ": H1 drawn " $ (H1.Draws > 0) $ ", the clip at row " $ ClipRow());
		SayCounts("W1", W1, K1);
		SayCounts("W2", W2, K2);
		SayCounts("W3", W3, K3);
		SayCounts("Flat", Flat, None);
		SayCounts("Outside", Outside, None);
		Step++;
		if (Step < 6)
		{
			Clip.SetChildPosition(0, Rows[Step]);
			Phase = 1;
		}
		else
			Phase = 3;
		StepTime = 0;
	}
	else if (Phase == 3 && StepTime > 0.5)
	{
		SayList("L1, straight in a clip window 60 high", L1);
		SayList("L2, in a window 300 high in a clip window 60 high", L2);
		SayList("L3, 60 high straight on a window 400 high", L3);
		SayList("L4, straight in a clip window 200 high cut to 100", L4);
		Edit.MoveInsertionPoint(MOVEINSERT_Home);
		Edit.MoveInsertionPoint(MOVEINSERT_PageDown);
		Log("DXCLIPDRAW: the edit field, in a window 300 high in a clip window 60 high: its clip rectangle's height "
			$ int(Edit.winClipRect.clipHeight) $ ", Home then Page Down to " $ Edit.GetInsertionPoint());
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 0.5)
	{
		Log("DXCLIPDRAW: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
