//=============================================================================
// RootBackConsole: how much of the screen the root window's own background
// and its raw background cover, shot in both engines (docs/DEVELOPMENT.md,
// scripted runs; bugs.md EXT.27). The original's root is the viewport over
// its multiplier (XRootWindow::ResizeRoot, Extension.dll 0x1003adf0), and
// it draws its background over all of it (XWindow::DrawTree 0x10051490,
// from PaintWindows 0x1003aab0) and its raw background over all of it less
// the scene (DrawRawBackground 0x1003c720). DXEngine's root is 4:3, by
// choice (bugs.md D.3), and draws both over the 4:3 root alone, so on a
// wider screen DeusExRootWindow.MaskBackground's MaskTexture -- modulated,
// the darkening under every menu and screen with the default UIBackground
// of 0 -- and ShowSnapshot's snapshot (UIBackground 1) leave the sides.
//
// On the menu map, 3 s in: the root's background Texture'Solid' in red,
// DSTY_Normal; 1 s later shot 0, marked for the original's grabber. Then
// the background taken away (None, DSTY_None), a raw background of
// Texture'Solid' in green, 32 by 32, stretched, and the rendering off; 1 s
// later shot 1. Then the root put back as the menu map has it (rendering on,
// no raw background, MaskBackground) and an exit. The root's and the
// screen's sizes are logged with "DXROOTBACK:" in front.
//
// Read, at DXCAP_WINDOW=1280x720 (a scale of 1; the original's root 1280
// wide, DXEngine's 960 at 160): the pixels at 40, 360 and 1240, 360, and
// at 200, 600 as the control. The original: red in shot 0, green in shot 1,
// at all three. DXEngine before a fix: the control red and green, the two
// at the sides the menu map's scene in shot 0 and black in shot 1.
//=============================================================================
class RootBackConsole extends Console;

var float RunTime;
var int Phase;
var bool bMarking;
var int MarkNumber;

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local Color Green;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root == None)
		return;
	if (Phase == 0 && RunTime > 3.0)
	{
		Log("DXROOTBACK: root " $ Root.width $ "x" $ Root.height $ ", screen " $ P.ConsoleCommand("getcurrentres"));
		Root.SetBackground(Texture'Solid');
		Root.SetBackgroundStyle(DSTY_Normal);
		Root.SetTileColorRGB(255, 0, 0);
		MarkNumber = 0;
		bMarking = true;
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 4.0)
	{
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		Root.SetBackground(None);
		Root.SetBackgroundStyle(DSTY_None);
		Root.SetTileColorRGB(255, 255, 255);
		Green.R = 0;
		Green.G = 255;
		Green.B = 0;
		Root.SetRawBackground(Texture'Solid', Green);
		Root.SetRawBackgroundSize(32, 32);
		Root.StretchRawBackground(True);
		Root.EnableRendering(False);
		MarkNumber = 1;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 5.0)
	{
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		Root.EnableRendering(True);
		Root.SetRawBackground(None);
		Root.StretchRawBackground(False);
		Root.MaskBackground(True);
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 6.0)
	{
		Log("DXROOTBACK: done, exiting");
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
