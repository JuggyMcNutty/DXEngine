//=============================================================================
// TileConsole: tiles and text drawn on the canvas and in a window, the same
// in both engines (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md,
// tiles). At Liberty Island's start, the HUD hidden: on the canvas, over a
// grey box, StaticLine and HackInfoBackground drawn STY_Modulated (their
// colour 0 black, and dark grey), "NEW GAME" in the menu's TrueType font
// (DXFonts.MainMenuTrueType, whose palette has alpha) and in
// FontMenuHeaders, and DrawTextClipped and DrawText STY_Translucent; in a
// CapTileWindow on the root, over its own grey box, the two drawn
// DSTY_Modulated, HUDScopeCrosshair (a texture itself masked) drawn
// DSTY_Normal, and the two fonts. Two shots 2 s apart, each marked for the
// original's grabber; then exit. Each step is logged with "DXTILE:" in
// front.
//=============================================================================
class TileConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Font TrueType;
var bool bDrawing;
var CapTileWindow Win;
var Object Stopped;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;
var bool bWidthsLogged;

function DrawTests(canvas C)
{
	C.bNoSmooth = True;
	C.Style = 1;
	C.DrawColor.R = 160;
	C.DrawColor.G = 160;
	C.DrawColor.B = 160;
	C.SetPos(20, 40);
	C.DrawRect(Texture'Solid', 600, 300);

	C.Style = 4;
	C.SetPos(40, 60);
	C.DrawTile(Texture'StaticLine', 128, 64, 0, 0, 16, 8);
	C.SetPos(200, 60);
	C.DrawTile(Texture'HackInfoBackground', 64, 64, 0, 0, 16, 16);

	C.Style = 1;
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;
	C.Font = TrueType;
	C.SetPos(300, 60);
	C.DrawText("NEW GAME", false);
	C.Font = Font'DeusExUI.FontMenuHeaders';
	C.SetPos(300, 120);
	C.DrawText("NEW GAME", false);

	C.Style = 3;
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 0;
	C.SetPos(40, 180);
	C.DrawTextClipped("CLIPPED");
	C.SetPos(40, 240);
	C.DrawText("WRAPPED", false);
	C.Style = 1;
	C.bNoSmooth = False;
}

// The width the canvas's font gives Latin-1 letters past ASCII (the stock
// fonts have them) and a space, logged once: a glyph index taken from a
// signed char once drew them as spaces on x86-64.
function LogWidths(canvas C)
{
	local float XL, YL, SL;

	C.Font = Font'DeusExUI.FontMenuHeaders';
	C.TextSize(" ", SL, YL);
	C.TextSize(Chr(233), XL, YL);
	Log("DXTILE: widths in FontMenuHeaders: space " $ SL $ ", e acute " $ XL);
	C.TextSize(Chr(252), XL, YL);
	Log("DXTILE: u umlaut " $ XL);
	C.TextSize(Chr(223), XL, YL);
	Log("DXTILE: sharp s " $ XL);
	C.TextSize(Chr(201), XL, YL);
	Log("DXTILE: E acute " $ XL);
	C.TextSize("Caf" $ Chr(233), XL, YL);
	Log("DXTILE: Cafe with the accent " $ XL);
	// Characters past the font's page
	C.TextSize("A", SL, YL);
	C.TextSize(Chr(256), XL, YL);
	Log("DXTILE: canvas widths: A " $ SL $ ", U+0100 " $ XL);
	C.TextSize(Chr(937), XL, YL);
	Log("DXTILE: U+03A9 " $ XL);
	C.TextSize("A" $ Chr(256) $ "A", XL, YL);
	Log("DXTILE: A U+0100 A " $ XL);
	C.StrLen("A" $ Chr(256) $ "A", XL, YL);
	Log("DXTILE: StrLen A U+0100 A " $ XL);
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (bDrawing && !bWidthsLogged)
	{
		bWidthsLogged = True;
		LogWidths(C);
	}
	if (bDrawing)
		DrawTests(C);
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

// A conversation's letterbox or a datalink's window would cover the tests.
function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXTILE: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXTILE: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None && InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0)
		Silence(DXP);

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXTILE: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = 1;
			}
		}
		else
		{
			MapTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 6.0 && DXP != None)
		{
			Root = DeusExRootWindow(DXP.rootWindow);
			if (Root == None)
				return;
			// The original's HUD draws over the grabber's mark.
			Root.ShowHud(False);
			TrueType = Font(DynamicLoadObject("DXFonts.MainMenuTrueType", class'Font'));
			Win = CapTileWindow(Root.NewChild(class'CapTileWindow'));
			Win.TrueType = TrueType;
			Win.SetPos(20, 380);
			Win.SetSize(600, 300);
			bDrawing = true;
			Log("DXTILE: the root " $ Root.width $ "x" $ Root.height $ ", the font " $ TrueType $ ", the window at "
				$ Win.x $ "," $ Win.y $ " " $ Win.width $ "x" $ Win.height);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// A shot every 2 s from 2 s on, the mark shown for the 0.6 s before
		// each.
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
				Log("DXTILE: shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			Log("DXTILE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
