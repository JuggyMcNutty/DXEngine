//=============================================================================
// TextBlockConsole: where a large text window puts its rows, shot in both
// engines (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, the UI). On
// the menu map, 3 s in: on the root, three large text windows 300 by 120 at
// 40, 40, at 360, 40 and at 680, 40, each with two lines of white text on a
// black box, aligned to the middle across and, in turn, to the top, the
// middle and the bottom; then a shot, marked for the original's grabber, and
// an exit. Each window's place, size and the font's height are logged with
// "DXTEXTBLOCK:" in front.
//=============================================================================
class TextBlockConsole extends Console;

var float RunTime;
var int Phase;
var bool bMarking;
var LargeTextWindow Boxes[3];

function MakeBox(DeusExRootWindow Root, int i, float X)
{
	local LargeTextWindow W;

	W = LargeTextWindow(Root.NewChild(class'LargeTextWindow'));
	W.SetPos(X, 40);
	W.SetSize(300, 120);
	W.SetBackground(Texture'Solid');
	W.SetBackgroundStyle(DSTY_Normal);
	W.SetTileColorRGB(0, 0, 0);
	W.SetTextColorRGB(255, 255, 255);
	W.SetFont(Font'FontMenuHeaders');
	if (i == 0)
		W.SetTextAlignments(HALIGN_Center, VALIGN_Top);
	else if (i == 1)
		W.SetTextAlignments(HALIGN_Center, VALIGN_Center);
	else
		W.SetTextAlignments(HALIGN_Center, VALIGN_Bottom);
	W.SetText("The first line" $ Chr(10) $ "and the second");
	W.Show();
	Boxes[i] = W;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local int i;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 3.0)
	{
		Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		if (Root == None)
			return;
		MakeBox(Root, 0, 40);
		MakeBox(Root, 1, 360);
		MakeBox(Root, 2, 680);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 4.0)
	{
		for (i = 0; i < 3; i++)
			Log("DXTEXTBLOCK: box " $ i $ " at " $ Boxes[i].x $ "," $ Boxes[i].y $ " " $ Boxes[i].width $ "x" $ Boxes[i].height);
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 5.0)
	{
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 6.0)
	{
		Log("DXTEXTBLOCK: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

// The mark for the original's screen grabber, as CaptureConsole's: a
// magenta block, then the shot's number (0) in eight black blocks
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
	C.DrawColor.R = 0;
	C.DrawColor.G = 0;
	C.DrawColor.B = 0;
	for (i = 0; i < 8; i++)
	{
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}
