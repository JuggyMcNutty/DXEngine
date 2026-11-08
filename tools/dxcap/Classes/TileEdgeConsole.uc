//=============================================================================
// TileEdgeConsole (bugs.md REN.51): the canvas's DrawTile and DrawTileClipped
// at the edges no stock script reaches (docs/DEVELOPMENT.md, scripted runs;
// docs/NATIVES.md, tiles). The original's execDrawTile (Engine.dll
// 0x10377ba0) logs a None texture and moves nothing; any other tile moves
// CurX on by SpaceX and its width, whatever its size, and CurYL to the
// larger of it and the tile's height, SpaceY no part of it; STY_Masked adds
// no flag, the canvas's DrawTile taking the texture's own masked bit
// (0x10376160). Its execDrawTileClipped (0x10378450) does nothing for a None
// texture or a tile of no width or height, makes a negative CurX or CurY 0
// with as much of the tile taken off, then cuts at ClipX and ClipY, and moves
// CurX and CurYL by the cut tile.
//
// Liberty Island opened from wherever the run starts; 6 s in, the HUD
// hidden: each case set up from the canvas's origin with its own CurX,
// CurY, CurYL, SpaceX 3 and SpaceY 7, drawn STY_None (the cursor moves as in
// any style), and CurX, CurY and CurYL after it logged -- DrawTile of None,
// of no width, of a negative width, and of a tile 20 by 30; DrawTileClipped
// from -10, -20, across ClipX, of no width, and of None. Then, drawn every
// frame over a grey box and shot twice, marked for the original's grabber:
// TextureName (DeusExUI.BeltIconCrowbar by default: 64 by 64, not masked,
// 96% of its texels colour 0, magenta), point sampled at x5, STY_Masked in
// box A (40, 60) and STY_Normal in box B (380, 60), each 320 by 320; box C
// (720, 60) the grey alone. Each line is logged with "DXTILEEDGE:" in
// front; then exit.
//
// Compare at DXCAP_WINDOW=1280x720, the original through D3DDrv: the lines
// diffed, and maskcount.py's count of the pixels not magenta in each box.
// The original: A and B alike, the crowbar's 4% alone not magenta, C all of
// it. DXEngine before the fix: the None tiles moving CurX, the tiles of no
// width or a negative one not, CurYL taking SpaceY, the clipped tile from
// -10, -20 leaving CurY at -20; A drawn masked, its colour 0 grey.
//=============================================================================
class TileEdgeConsole extends Console;

var config string TextureName;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var bool bDrawing;
var bool bMeasured;
var Texture Tex;
var Object Stopped;

var int MarkShot;
var bool bMarking;

function Say(string S)
{
	Log("DXTILEEDGE: " $ S);
}

function string TextureOf()
{
	if (TextureName == "")
		return "DeusExUI.BeltIconCrowbar";
	return TextureName;
}

// A case's start: the origin 0, 0, the clip 640 by 480, the cursor where it
// is asked to be, SpaceX 3, SpaceY 7, nothing drawn
function Prep(canvas C, float X, float Y, float YL)
{
	C.Style = 0;
	C.SetOrigin(0, 0);
	C.SetClip(640, 480);
	C.SpaceX = 3;
	C.SpaceY = 7;
	C.CurX = X;
	C.CurY = Y;
	C.CurYL = YL;
}

function Report(canvas C, string Tag)
{
	Say(Tag $ ": CurX " $ C.CurX $ " CurY " $ C.CurY $ " CurYL " $ C.CurYL);
}

function Measure(canvas C)
{
	Prep(C, 100, 100, 0);
	C.DrawTile(None, 10, 10, 0, 0, 8, 8);
	Report(C, "DrawTile None");

	Prep(C, 100, 100, 0);
	C.DrawTile(Tex, 0, 10, 0, 0, 8, 8);
	Report(C, "DrawTile no width");

	Prep(C, 100, 100, 0);
	C.DrawTile(Tex, -5, 10, 0, 0, 8, 8);
	Report(C, "DrawTile negative width");

	Prep(C, 100, 100, 0);
	C.DrawTile(Tex, 20, 30, 0, 0, 8, 8);
	Report(C, "DrawTile 20x30");

	Prep(C, -10, -20, 0);
	C.DrawTileClipped(Tex, 50, 60, 0, 0, 50, 60);
	Report(C, "DrawTileClipped from -10,-20");

	Prep(C, 600, 0, 0);
	C.DrawTileClipped(Tex, 100, 10, 0, 0, 64, 64);
	Report(C, "DrawTileClipped across ClipX");

	Prep(C, 100, 100, 0);
	C.DrawTileClipped(Tex, 0, 10, 0, 0, 8, 8);
	Report(C, "DrawTileClipped no width");

	Prep(C, 100, 100, 0);
	C.DrawTileClipped(None, 10, 10, 0, 0, 8, 8);
	Report(C, "DrawTileClipped None");

	C.SpaceX = 0;
	C.SpaceY = 0;
	C.CurYL = 0;
	C.SetClip(C.SizeX, C.SizeY);
}

function DrawCases(canvas C)
{
	C.bNoSmooth = True;
	C.Style = 1;
	C.SpaceX = 0;
	C.SpaceY = 0;
	C.SetOrigin(0, 0);
	C.SetClip(C.SizeX, C.SizeY);
	C.DrawColor.R = 128;
	C.DrawColor.G = 128;
	C.DrawColor.B = 128;
	C.SetPos(20, 40);
	C.DrawRect(Texture'Solid', 1040, 360);

	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;
	// A: STY_Masked, the texture not masked: drawn whole in the original
	C.Style = 2;
	C.SetPos(40, 60);
	C.DrawTile(Tex, 320, 320, 0, 0, 64, 64);
	// B: STY_Normal, the control
	C.Style = 1;
	C.SetPos(380, 60);
	C.DrawTile(Tex, 320, 320, 0, 0, 64, 64);
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
	C.bNoSmooth = True;
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
	C.bNoSmooth = False;
}

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Say("datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Say("conversation " $ Stopped.Name $ " stopped");
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
				Say("opening 01_NYC_UNATCOIsland");
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
			Tex = Texture(DynamicLoadObject(TextureOf(), class'Texture'));
			if (Tex == None)
			{
				Say("no " $ TextureOf() $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Say("texture " $ Tex $ " " $ Tex.USize $ "x" $ Tex.VSize $ " bMasked " $ Tex.bMasked);
			Say("tiles: A at 40,60 STY_Masked, B at 380,60 STY_Normal, each 320x320 of 64x64 texels, bNoSmooth; C at 720,60 the grey box");
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
				Say("shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			Say("done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
