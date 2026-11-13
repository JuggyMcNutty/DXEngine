//=============================================================================
// RawBackArgsConsole (bugs.md EXT.36): the root window's SetRawBackground
// with an argument left out, shot in both engines. The original draws the
// raw background only behind a window (its clip list; with no window up,
// the menu map's root draws none: RootBackConsole's raw background black in
// its shot), so this runs behind the main menu, as the Snapshot option shows
// its picture (SnapSmoothConsole). From the menu map to 01_NYC_UNATCOIsland;
// 4 s after it opens the player's UIBackground set to 1 and the main menu
// opened as Esc opens it (DeusExPlayer.ShowMainMenu: the snapshot made the
// raw background); 1 s later a raw background of Texture'Solid' in green,
// 32 by 32, stretched; 1 s later shot 0. Then SetRawBackground(Texture'Solid'),
// its colour left out; 1 s later shot 1. Then SetRawBackground(), both left
// out; 1 s later shot 2. Then the menus closed, UIBackground put back to 0
// and an exit. Each step logged with "DXRAWARGS:" in front; each shot marked
// for the original's grabber.
//
// The original's native gives a texture left out None and a colour left out
// white (Extension.dll 0x1003cbe0, into XRootWindow::SetRawBackground
// 0x10039eb0). At DXCAP_WINDOW=1280x720 with DXCAP_RENDERER=D3D both
// engines draw the raw background over 320 by 180 at the frame's top left;
// read the pixels at 100, 100 and 300, 170: the original's green in shot 0,
// white in shot 1, none drawn (black) in shot 2. DXEngine before a fix kept
// what it had: green in all three.
//=============================================================================
class RawBackArgsConsole extends Console;

var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var bool bMarking;
var int MarkNumber;

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
	local Color Green;
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
	if (DXP != None)
		Root = DeusExRootWindow(DXP.rootWindow);

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
	else if (Phase == 1 && M ~= "01_NYC_UNATCOIsland" && MapTime > 4.0 && Root != None)
	{
		Log("DXRAWARGS: root " $ Root.width $ "x" $ Root.height $ ", screen " $ P.ConsoleCommand("getcurrentres"));
		DXP.UIBackground = 1;
		DXP.ShowMainMenu();
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		Green.R = 0;
		Green.G = 255;
		Green.B = 0;
		Root.SetRawBackground(Texture'Solid', Green);
		Root.SetRawBackgroundSize(32, 32);
		Root.StretchRawBackground(True);
		Log("DXRAWARGS: Solid in green");
		MarkNumber = 0;
		bMarking = true;
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 1.0)
	{
		Log("DXCAP: shot 0");
		P.ConsoleCommand("shot");
		bMarking = false;
		Root.SetRawBackground(Texture'Solid');
		Log("DXRAWARGS: Solid, the colour left out");
		MarkNumber = 1;
		bMarking = true;
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 1.0)
	{
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		Root.SetRawBackground();
		Log("DXRAWARGS: both left out");
		MarkNumber = 2;
		bMarking = true;
		StepTime = 0;
		Phase = 5;
	}
	else if (Phase == 5 && StepTime > 1.0)
	{
		Log("DXCAP: shot 2");
		P.ConsoleCommand("shot");
		bMarking = false;
		Root.ClearWindowStack();
		DXP.UIBackground = 0;
		StepTime = 0;
		Phase = 6;
	}
	else if (Phase == 6 && StepTime > 1.0)
	{
		Log("DXRAWARGS: done, exiting");
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
