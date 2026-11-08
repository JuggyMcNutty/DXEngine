//=============================================================================
// CanvasTextConsole: the canvas's text natives on strings no stock script
// draws, measured (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md,
// tiles). Liberty Island opened from wherever the run starts; 6 s in, the
// HUD hidden, in Engine's MedFont, STY_Translucent: each case set up from
// the canvas's origin (0, 0) with its own SetPos, SetClip, SpaceX and
// bCenter, then StrLen's or TextSize's XL and YL, or DrawText's CurX, CurY
// and CurYL after it, logged -- a line break inside the text, a word wider
// than the clip, a character the font has at no width (a no-break space, a
// tab), the space a wrap falls on, StrLen after a SetPos off whole pixels, a
// centred line from a CurX not 0, two wrapped lines with no CR; then, drawn
// every frame over a grey box and shot twice, marked for the original's
// grabber, DrawTextClipped with its last letter across ClipX, with SpaceX
// 3, and with the hot keys "&&X" and "&File". Each line is logged with
// "DXTEXT:" in front; then exit.
//=============================================================================
class CanvasTextConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var bool bDrawing;
var bool bMeasured;
var Object Stopped;

var int MarkShot;
var bool bMarking;

function Prep(canvas C, float X, float Y, float CX, float CY, float SX, bool bCenterIt)
{
	C.Font = C.MedFont;
	C.Style = 3;
	C.SpaceX = SX;
	C.SpaceY = 0;
	C.CurYL = 0;
	C.bCenter = bCenterIt;
	C.SetOrigin(0, 0);
	C.SetClip(CX, CY);
	C.SetPos(X, Y);
}

function LogStrLen(canvas C, string Tag, string S)
{
	local float XL, YL;
	C.StrLen(S, XL, YL);
	Log("DXTEXT: " $ Tag $ ": StrLen " $ XL $ " " $ YL $ ", CurX " $ C.CurX $ " CurY " $ C.CurY);
}

function LogTextSize(canvas C, string Tag, string S)
{
	local float XL, YL;
	C.TextSize(S, XL, YL);
	Log("DXTEXT: " $ Tag $ ": TextSize " $ XL $ " " $ YL);
}

function LogDraw(canvas C, string Tag, string S, bool CR)
{
	C.DrawText(S, CR);
	Log("DXTEXT: " $ Tag $ ": after DrawText CurX " $ C.CurX $ " CurY " $ C.CurY $ " CurYL " $ C.CurYL);
}

function Measure(canvas C)
{
	local string NL;

	NL = Chr(10);
	Prep(C, 10, 100, 600, 400, 0, False);
	LogStrLen(C, "line break", "AB" $ NL $ "CD");
	Prep(C, 10, 100, 600, 400, 0, False);
	LogDraw(C, "line break drawn", "AB" $ NL $ "CD", False);
	Prep(C, 10, 100, 600, 400, 0, False);
	LogDraw(C, "line break first", NL $ "CD", False);

	Prep(C, 0, 100, 100, 400, 0, False);
	LogStrLen(C, "long word", "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	Prep(C, 0, 100, 100, 400, 0, False);
	LogDraw(C, "long word drawn", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", False);
	Prep(C, 0, 100, 4, 400, 0, False);
	LogDraw(C, "first glyph too wide", "AB", False);

	Prep(C, 0, 100, 600, 400, 0, False);
	LogTextSize(C, "no-break space", "A" $ Chr(160) $ "B");
	LogTextSize(C, "tab", "A" $ Chr(9) $ "B");
	LogTextSize(C, "plain", "AB");
	LogStrLen(C, "no-break space", "A" $ Chr(160) $ "B");

	Prep(C, 0, 100, 40, 400, 0, False);
	LogStrLen(C, "wrap on a space", "HELLO WORLD");
	Prep(C, 0, 100, 40, 400, 0, False);
	LogStrLen(C, "wrap on two spaces", "HELLO  WORLD");

	Prep(C, 10.75, 20.5, 600, 400, 0, False);
	LogStrLen(C, "off whole pixels", "A");

	Prep(C, 100, 300, 600, 400, 0, True);
	LogDraw(C, "centred from 100", "ABC", False);
	Prep(C, 0, 300, 600, 400, 0, True);
	LogDraw(C, "centred from 0", "ABC", False);

	Prep(C, 30, 300, 60, 400, 0, False);
	LogDraw(C, "two lines no CR", "ABCDEF GH", False);
	Prep(C, 30, 300, 60, 400, 0, False);
	LogDraw(C, "two lines CR", "ABCDEF GH", True);

	Prep(C, 0, 0, C.SizeX, C.SizeY, 0, False);
}

function DrawCases(canvas C)
{
	C.bNoSmooth = True;
	C.Style = 1;
	C.SetOrigin(0, 0);
	C.SetClip(C.SizeX, C.SizeY);
	C.DrawColor.R = 128;
	C.DrawColor.G = 128;
	C.DrawColor.B = 128;
	C.SetPos(20, 40);
	C.DrawRect(Texture'Solid', 600, 300);
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 0;
	C.Font = C.MedFont;
	C.Style = 3;
	C.SpaceX = 0;
	// The last letter across ClipX: ClipX 100 from an origin at 40, 60
	C.SetOrigin(40, 60);
	C.SetClip(100, 40);
	C.SetPos(0, 0);
	C.DrawTextClipped("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	C.SetOrigin(0, 0);
	C.SetClip(C.SizeX, C.SizeY);
	C.SpaceX = 3;
	C.SetPos(40, 120);
	C.DrawTextClipped("SPACED");
	C.SetPos(40, 160);
	C.DrawText("SPACED", False);
	C.SpaceX = 0;
	C.SetPos(40, 200);
	C.DrawTextClipped("&&X &File", True);
	C.SetPos(40, 240);
	C.DrawText("AB" $ Chr(10) $ "CD", False);
	C.Style = 1;
	C.bNoSmooth = False;
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (bDrawing && !bMeasured)
	{
		bMeasured = True;
		Measure(C);
	}
	if (bDrawing)
		DrawCases(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetOrigin(0, 0);
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

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXTEXT: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXTEXT: conversation " $ Stopped.Name $ " stopped");
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
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXTEXT: opening 01_NYC_UNATCOIsland");
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
			Root.ShowHud(False);
			bDrawing = true;
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
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
				Log("DXTEXT: shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			Log("DXTEXT: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
