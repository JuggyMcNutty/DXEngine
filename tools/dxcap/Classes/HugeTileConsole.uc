//=============================================================================
// HugeTileConsole (bugs.md REN.54): a wrapped tile whose texture
// coordinate lies past 2^31 texels. The software device's SIMD sampler
// (SoftwarePipelines.h, SoftwarePixel::Bilinear and Nearest) maps only a
// coordinate of 1e30 or more to 0 (FiniteOrZero); between 2^31 and 1e30 its
// Floor and Truncate (cvttps2dq, and NEON's and the loops' copies of it)
// give INT_MIN, the wrap leaves the column near -2^31, and the texel read
// lands some 8 GiB before the texture. Its float reference (DXE_SW_REFERENCE=1,
// SoftwareSampler::Frac) samples texel 0 there; the GPU devices and D3DDrv
// wrap in hardware.
//
// At Liberty Island's start, the HUD hidden: two tiles of TextureName
// (Engine.DefaultTexture by default) 64 by 64, STY_Normal, smooth: A with
// U = 1e6 (under 2^31 texels: the control) at 40,60, then B with U = 1e12
// (u = U / USize, past 2^31) at 140,60. B is drawn from the second frame
// after A has been shot, so a run that stops leaves A's shot. Each line is
// logged with "DXHUGE:" in front. Expected: the original (and DXEngine on
// Vulkan, GL, or the software device with DXE_SW_REFERENCE=1) draws both,
// logs "shot 1" and exits; the software device with its pipelines stops at
// B's first frame (SIGSEGV), its log ending after "drawing B".
//=============================================================================
class HugeTileConsole extends Console;

var config string TextureName;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var bool bDrawA;
var bool bDrawB;
var Texture Tex;

var int MarkShot;
var bool bMarking;

function Say(string S)
{
	Log("DXHUGE: " $ S);
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	C.Style = 1;
	C.bNoSmooth = False;
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;
	if (bDrawA)
	{
		C.SetPos(40, 60);
		C.DrawTile(Tex, 64, 64, 1000000.0, 0, 64, 64);
	}
	if (bDrawB)
	{
		C.SetPos(140, 60);
		C.DrawTile(Tex, 64, 64, 1000000000000.0, 0, 64, 64);
	}
	if (!bMarking)
		return;
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
	local string TexName;

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
			TexName = TextureName;
			if (TexName == "")
				TexName = "Engine.DefaultTexture";
			Tex = Texture(DynamicLoadObject(TexName, class'Texture'));
			if (Tex == None)
			{
				Say("no " $ TexName $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Say("texture " $ Tex $ " " $ Tex.USize $ "x" $ Tex.VSize $ "; A at U 1e6 (u " $ (1000000.0 / Tex.USize) $ "), B at U 1e12 (u " $ (1000000000000.0 / Tex.USize) $ ")");
			bDrawA = true;
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (StepTime > 2.0)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 0.0;
			}
			else
			{
				Say("shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
				if (ShotsTaken == 1)
				{
					Say("drawing B");
					bDrawB = true;
				}
				else
				{
					Say("done, exiting");
					P.ConsoleCommand("exit");
					Phase = 9;
				}
			}
		}
	}
}
