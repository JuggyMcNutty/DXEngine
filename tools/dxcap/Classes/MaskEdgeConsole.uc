//=============================================================================
// MaskEdgeConsole (bugs.md REN.52): a masked texture's edges as the
// alpha test cuts them, in both engines. D3DDrv's SetRes sets ALPHAREF 0x7F
// with ALPHAFUNC GREATER (D3DDrv.dll 0x1000c83a, 0x1000c84a) and SetBlending
// turns the test on with PF_Masked (0x10008441-0x10008454): a texel shows
// when its filtered alpha is above 127/255, as every DXEngine device keeps
// it (Scene.frag's ALPHATEST, SoftwarePipelines.h's alpha test); a test at
// 0.5 cut a filtered alpha in (127/255, 0.5): about 0.85% of a bilinear cell
// where two diagonal texels are opaque, 0.15% and 0.12% where one or three
// are, none along a straight edge.
//
// At Liberty Island's start, the HUD hidden: over a magenta box, three tiles
// of TextureName (CoreTexMetal.Metal.ClenChainlink_B by default, the chain
// link the statue's pedestal holds, masked) drawn STY_Masked in white, each
// 320 by 320 pixels: A, its first 16 by 16 texels smooth (x20); B, its first
// 64 by 64 smooth (x5); C, A's texels with bNoSmooth (point sampled: the
// control, alike in both engines). Two shots 2 s apart, each marked for the
// original's grabber; then exit. Each line is logged with "DXMASK:" in
// front.
//
// Compare (each engine's shot, the original's with DXCAP_RENDERER=D3D): the
// pixels not magenta in each tile's box, A's, B's and C's alike; with a test
// at 0.5, A's and B's lower in DXEngine's, by some 3 pixels a diagonal joint
// in A.
//=============================================================================
class MaskEdgeConsole extends Console;

var config string TextureName;
var config float MarkTime;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var bool bDrawing;
var Texture Tex;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function Say(string S)
{
	Log("DXMASK: " $ S);
}

function string TextureOf()
{
	if (TextureName == "")
		return "CoreTexMetal.Metal.ClenChainlink_B";
	return TextureName;
}

function float Mark()
{
	if (MarkTime <= 0)
		return 2.0;
	return MarkTime;
}

function DrawTests(canvas C)
{
	// The magenta box behind the three tiles
	C.bNoSmooth = True;
	C.Style = 1;
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.SetPos(20, 40);
	C.DrawRect(Texture'Solid', 1040, 360);

	C.Style = 2; // STY_Masked
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;

	// A: 16 by 16 texels, smooth, x20
	C.bNoSmooth = False;
	C.SetPos(40, 60);
	C.DrawTile(Tex, 320, 320, 0, 0, 16, 16);
	// B: 64 by 64 texels, smooth, x5
	C.SetPos(380, 60);
	C.DrawTile(Tex, 320, 320, 0, 0, 64, 64);
	// C: A's texels point sampled
	C.bNoSmooth = True;
	C.SetPos(720, 60);
	C.DrawTile(Tex, 320, 320, 0, 0, 16, 16);

	C.Style = 1;
	C.bNoSmooth = False;
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (bDrawing)
		DrawTests(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.bNoSmooth = True;
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

	if (Phase == 0)
	{
		// The original starts in its menu map.
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
			// The original's HUD draws over the grabber's mark and the box.
			Root.ShowHud(False);
			// A conversation's letterbox would cover the box (TileConsole's
			// Silence would do the same here)
			if (DXP.dataLinkPlay != None)
				DXP.dataLinkPlay.AbortDataLink();
			Tex = Texture(DynamicLoadObject(TextureOf(), class'Texture'));
			if (Tex == None)
			{
				Say("no " $ TextureOf() $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Say("texture " $ Tex $ " " $ Tex.USize $ "x" $ Tex.VSize $ " bMasked " $ Tex.bMasked);
			Say("tiles: A at 40,60 (16x16 texels, smooth), B at 380,60 (64x64, smooth), C at 720,60 (16x16, bNoSmooth), each 320x320");
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
				StepTime = 2.0 - Mark();
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
