//=============================================================================
// SnapSmoothConsole: the menus' snapshot background, filtered or not, shot
// in both engines (docs/DEVELOPMENT.md, scripted runs; bugs.md EXT.29, EXT.27). The
// original's root draws its raw background through its own GC
// (XRootWindow::PaintWindows 0x1003aab0, DrawRawBackground 0x1003c720:
// drawing on, DSTY_Normal, the raw colour, and the smoothing left as the
// root's GC has it), and DeusExRootWindow.ShowSnapshot turns that GC's
// smoothing on (SetBackgroundSmoothing, XWindow::SetBackgroundSmoothing
// 0x1004d700) before it hands it the 256 by 192 snapshot, stretched. So
// with UIBackground 1 (Snapshot) the original's menus lie over a filtered
// picture; DXEngine draws it unsmoothed (Render/RenderWindow.cpp,
// DrawRawBackground), each snapshot pixel a flat block.
//
// From the menu map to 01_NYC_UNATCOIsland; 4 s after it opens the player's
// UIBackground set to 1 and the main menu opened as Esc opens it
// (DeusExPlayer.ShowMainMenu, which pauses and takes the snapshot); 1.5 s
// later a shot, marked for the original's grabber, the snapshot's size and
// the root's background smoothing and stretching logged with "DXSNAP:" in
// front; then the menus closed, UIBackground put back to 0, and an exit.
//
// Read, at DXCAP_WINDOW=1280x720 with DXCAP_RENDERER=D3D: rows 300 to 420,
// columns 20 to 150 (beside the menu): the share of horizontally adjacent
// pixels that are equal. DXEngine's snapshot, 5 pixels a texel unfiltered:
// some four in five equal; the original's, filtered, far fewer.
//=============================================================================
class SnapSmoothConsole extends Console;

var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var bool bMarking;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow Root;
	local string M;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	M = MapOf(P);
	if (M != CurrentMap)
	{
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1 && M ~= "01_NYC_UNATCOIsland" && MapTime > 4.0)
	{
		DXP.UIBackground = 1;
		DXP.ShowMainMenu();
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		Root = DeusExRootWindow(DXP.rootWindow);
		// RootWindow's raw background fields are private: the snapshot's
		// size and the root's smoothing, which Window keeps readable
		if (Root != None)
			Log("DXSNAP: snapshot " $ Root.snapshotWidth $ "x" $ Root.snapshotHeight $ ", background smoothing " $ Root.bSmoothBackground $ ", stretching " $ Root.bStretchBackground);
		bMarking = true;
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 0.5)
	{
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		Root = DeusExRootWindow(DXP.rootWindow);
		if (Root != None)
			Root.ClearWindowStack();
		DXP.UIBackground = 0;
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 1.0)
	{
		Log("DXSNAP: done, exiting");
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
