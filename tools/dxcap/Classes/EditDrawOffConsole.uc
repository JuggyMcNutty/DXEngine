//=============================================================================
// EditDrawOffConsole: an edit window's insertion point while its GC's drawing
// is off, shot in both engines (docs/DEVELOPMENT.md, scripted runs; bugs.md
// EXT.35). The original's XEditWindow::Draw (Extension.dll 0x10020d00),
// editable and with the focus, draws its rows through the GC its script's
// DrawWindow left, then its insertion point after a SetStyle(DSTY_Masked),
// which turns drawing on (XGC::SetStyle 0x10027170): a field whose
// DrawWindow turns drawing off shows its insertion point and none of its
// text. DXEngine before a fix drew neither.
//
// On the menu map, 3 s in, with no modal up (the root takes the focus, and
// no pointer is drawn): on the root a dark grey box at 20, 20, 400 by 120
// (Texture'Solid', 64, 64, 64) holding two CapEditDrawOffWindows, 200 by 20,
// at 10, 10 ("off", its DrawWindow turning drawing off) and at 10, 60 ("on",
// the control), each MMMMMMMM in green with its insertion point, a white
// bar 4 wide, at the end. 0.5 s later the focus put on "off"; 1 s later
// shot 0, marked for the original's grabber. Then the focus put on "on"; 1 s
// later shot 1. Then the box is destroyed and the run exits. Each window's
// place on the screen and the focus at each shot are logged with
// "DXEDITOFF:" in front.
//
// Read, at DXCAP_WINDOW=1024x768 (a scale of 1), in each window's rectangle
// as logged: the pixels of the text's green (0, 255, 0) and of the
// insertion point's white (255, 255, 255). The original: in shot 0 "off"
// the white bar and no green, "on" green and no white; in shot 1 "off"
// neither, "on" both. DXEngine before a fix: "off" neither in both shots.
//=============================================================================
class EditDrawOffConsole extends Console;

var float RunTime;
var float StepTime;
var int Phase;
var bool bMarking;
var int MarkNumber;
var Window Box;
var CapEditDrawOffWindow EditOff, EditOn;

function CapEditDrawOffWindow MakeEdit(float Y, bool bOff)
{
	local CapEditDrawOffWindow E;

	E = CapEditDrawOffWindow(Box.NewChild(class'CapEditDrawOffWindow'));
	E.SetPos(10, Y);
	E.SetSize(200, 20);
	E.Setup(bOff);
	E.Show();
	return E;
}

function LogPlace(string Label, Window W, Window Root)
{
	local float X, Y;

	W.ConvertCoordinates(W, 0, 0, Root, X, Y);
	Log("DXEDITOFF: " $ Label $ " at " $ X $ "," $ Y $ " " $ W.width $ "x" $ W.height);
}

function string FocusName(Window Root)
{
	local Window F;

	F = Root.GetFocusWindow();
	if (F == EditOff)
		return "off";
	if (F == EditOn)
		return "on";
	if (F == None)
		return "None";
	return string(F.Class.Name);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	StepTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root == None)
		return;
	if (Phase == 0 && RunTime > 3.0)
	{
		Box = Root.NewChild(class'Window');
		Box.SetPos(20, 20);
		Box.SetSize(400, 120);
		Box.SetBackground(Texture'Solid');
		Box.SetBackgroundStyle(DSTY_Normal);
		Box.SetTileColorRGB(64, 64, 64);
		Box.Show();
		EditOff = MakeEdit(10, True);
		EditOn = MakeEdit(60, False);
		StepTime = 0;
		Phase = 1;
	}
	else if (Phase == 1 && StepTime > 0.5)
	{
		Root.SetFocusWindow(EditOff);
		MarkNumber = 0;
		bMarking = true;
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		LogPlace("off", EditOff, Root);
		LogPlace("on", EditOn, Root);
		Log("DXEDITOFF: shot 0, focus " $ FocusName(Root));
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		Root.SetFocusWindow(EditOn);
		MarkNumber = 1;
		bMarking = true;
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 1.0)
	{
		Log("DXEDITOFF: shot 1, focus " $ FocusName(Root));
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		Box.Destroy();
		Box = None;
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 0.5)
	{
		Log("DXEDITOFF: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

// The mark for the original's screen grabber, as CaptureConsole's: a
// magenta block, then the shot's number in eight blocks, black for a 0 bit
// and white for a 1, the low bit first
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
		if ((MarkNumber & (1 << i)) != 0)
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
