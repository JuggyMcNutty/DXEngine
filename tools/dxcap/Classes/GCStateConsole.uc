//=============================================================================
// GCStateConsole: the state of the GC a window's DrawWindow and
// PostDrawWindow are handed, logged with "DXGCSTATE:" (docs/DEVELOPMENT.md,
// scripted runs; bugs.md EXT.31). The original's window draws through a GC of
// its own (XWindow::Init 0x1004bb30) that the window's setters set as they
// are called: SetBackgroundStyle its style (0x1004d670), SetBackground
// Smoothing its smoothing (0x1004d700); a GC's EnableMasking,
// EnableTranslucency and EnableModulation make its style again
// (XGC::GenerateStyle 0x1002aed0); and DrawTree (0x10051490) pushes it once
// round the window's Draw, its children and its PostDraw, so PostDrawWindow
// takes what DrawWindow left.
//
// On the menu map, 3 s in: on the root four CapGCStateWindows 40 by 40 at
// 100, 100, 160, 100, 220, 100 and 280, 100, none given a background
// texture: "plain" (as made), "translucent" (SetBackgroundStyle
// DSTY_Translucent), "smoothed" (SetBackgroundSmoothing True), "none"
// (SetBackgroundStyle DSTY_None). Each logs its first two draws. 2 s later
// the windows are destroyed and the run exits.
//
// The original, read from its code: plain style 1 (DSTY_Normal), smoothing
// False; translucent style 3, translucency True; smoothed smoothing True;
// none style 0, drawing False; after EnableTranslucency style 3, but 0 for
// none, whose drawing stays off (GenerateStyle gives 0 with drawing off);
// post style 4, modulation True, font FontMenuSmall, for each.
// DXEngine before a fix: style 0 and every flag False but drawing True for
// all four, the style 0 after EnableTranslucency, and post style 0,
// modulation False, the window's own font.
//=============================================================================
class GCStateConsole extends Console;

var float RunTime;
var int Phase;
var CapGCStateWindow Windows[4];

function CapGCStateWindow Make(DeusExRootWindow Root, int i, string Label)
{
	local CapGCStateWindow W;

	W = CapGCStateWindow(Root.NewChild(class'CapGCStateWindow'));
	W.Label = Label;
	W.SetPos(100 + 60 * i, 100);
	W.SetSize(40, 40);
	W.Show();
	return W;
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
		Windows[0] = Make(Root, 0, "plain");
		Windows[1] = Make(Root, 1, "translucent");
		Windows[1].SetBackgroundStyle(DSTY_Translucent);
		Windows[2] = Make(Root, 2, "smoothed");
		Windows[2].SetBackgroundSmoothing(True);
		Windows[3] = Make(Root, 3, "none");
		Windows[3].SetBackgroundStyle(DSTY_None);
		Log("DXGCSTATE: the windows made");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 5.0)
	{
		for (i = 0; i < 4; i++)
			Windows[i].Destroy();
		Log("DXGCSTATE: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
