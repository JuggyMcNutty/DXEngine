//=============================================================================
// ShortMipConsole: textures whose DXT1 mips are short of their bytes, drawn
// on the canvas (bugs.md REN.38). The textures are CapDXT1.utx's, which
// tools/dxcap/testdxt1.py writes into a folder a dxe run finds by
// DXCAP_SERVERPKGS (its paths; a game standing alone serves nothing):
// CapDXT1Whole, the control, green; CapDXT1ShortTop, its top mip half its
// bytes; CapDXT1ShortLower, its 32x32 mip half its bytes, red, then yellow,
// then cyan; CapDXT1Wide, 32768x4; CapDXT1Big, 2048x8. On the map the run
// starts on, 6 s in, the HUD hidden, conversations and datalinks stopped as
// they start: each texture loaded (Only: that one alone), then drawn on a
// black screen, each place logged -- the short lower one at 128, 22 and 11
// pixels, its top, its short level and the one under it, about; a shot 2 s
// later, an exit 1 s after it. DXEngine alone: each line starts "DXSHORTMIP:".
//=============================================================================
class ShortMipConsole extends Console;

var config string Only;

var string TexNames[5];
var Texture Tex[5];
var float MapTime, StepTime;
var int Phase;
var bool bDrawing;
var bool bPlacesLogged;
var Object Stopped;

function Say(string S)
{
	Log("DXSHORTMIP: " $ S);
}

function bool Wanted(int i)
{
	return Only == "" || Only ~= TexNames[i];
}

function LoadTextures()
{
	local int i;

	for (i = 0; i < 5; i++)
	{
		if (Wanted(i))
		{
			Tex[i] = Texture(DynamicLoadObject("CapDXT1." $ TexNames[i], class'Texture'));
			if (Tex[i] != None)
				Say(TexNames[i] $ " loaded: " $ Tex[i].USize $ "x" $ Tex[i].VSize);
			else
				Say(TexNames[i] $ " not loaded");
		}
	}
}

// One texture as a tile, the whole of it, at X, Y, W by H pixels
function DrawOne(canvas C, int i, float X, float Y, float W, float H)
{
	if (Tex[i] == None)
		return;
	C.SetPos(X, Y);
	C.DrawTile(Tex[i], W, H, 0, 0, Tex[i].USize, Tex[i].VSize);
	if (!bPlacesLogged)
		Say(TexNames[i] $ " drawn at " $ int(X) $ "," $ int(Y) $ " " $ int(W) $ "x" $ int(H));
}

event PostRender(canvas C)
{
	Super.PostRender(C);
	if (!bDrawing)
		return;
	C.bNoSmooth = False;
	C.Style = 1;
	C.DrawColor.R = 0;
	C.DrawColor.G = 0;
	C.DrawColor.B = 0;
	C.SetPos(0, 0);
	C.DrawRect(Texture'Solid', C.ClipX, C.ClipY);
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;
	DrawOne(C, 0, 40, 40, 128, 128);
	DrawOne(C, 1, 190, 40, 128, 128);
	DrawOne(C, 2, 340, 40, 128, 128);
	DrawOne(C, 2, 490, 40, 22, 22);
	DrawOne(C, 2, 530, 40, 11, 11);
	DrawOne(C, 3, 40, 200, 512, 16);
	DrawOne(C, 4, 40, 240, 512, 16);
	bPlacesLogged = True;
}

// A conversation's letterbox or a datalink's window would cover the tiles.
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
	if (DXP != None)
		Silence(DXP);

	if (Phase == 0 && MapTime > 6.0 && DXP != None)
	{
		Root = DeusExRootWindow(DXP.rootWindow);
		if (Root != None)
			Root.ShowHud(False);
		Say("on " $ P.Level.GetLocalURL() $ ", Only '" $ Only $ "'");
		LoadTextures();
		bDrawing = True;
		StepTime = 0;
		Phase = 1;
	}
	else if (Phase == 1 && StepTime > 2.0)
	{
		Say("shot 0");
		P.ConsoleCommand("shot");
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		Say("exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

defaultproperties
{
	TexNames(0)="CapDXT1Whole"
	TexNames(1)="CapDXT1ShortTop"
	TexNames(2)="CapDXT1ShortLower"
	TexNames(3)="CapDXT1Wide"
	TexNames(4)="CapDXT1Big"
}
