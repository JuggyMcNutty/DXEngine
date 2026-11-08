//=============================================================================
// SineTableConsole: the sine table every fractal texture shares, seen
// through a wet texture whose drops draw no random bytes (docs/DEVELOPMENT.md,
// scripted runs; docs/NATIVES.md, fire, water and ice textures). Liberty
// Island opened from wherever the run starts; 6 s in, the HUD hidden,
// TexName (Effects.Electricity.Xplsn_EMPG by default, the EMP grenade's
// blast: eleven WhirlyThing drops over Xplsn_Emp) loaded by its own class,
// WetTexture -- by Texture neither engine finds it -- and before it is
// first drawn its MaxFrameRate and MinFrameRate set to 0.01, so no paced
// step comes for 100 s, and its PrimeCount to Steps (100 by default) from a
// PrimeCurrent of 0: its first draw runs exactly that many steps in either
// engine (UTexture::Tick, Engine.dll 0x103ed0d0) and none after. Drawn every
// frame from then on, 512 by 512 at (64, 64), four pixels a texel,
// unsmoothed, STY_Normal; two shots 2 s apart, each marked for the
// original's grabber, the texture's PrimeCurrent logged at each; then exit.
// Each line is logged with "DXSINE:" in front. The shots' texels are read
// against the texture stepped with the table rounded to the nearest, as
// Fire.dll's fistp makes it (0x1050106a), and with it truncated (bugs.md
// REN.68).
//=============================================================================
class SineTableConsole extends Console;

var config string TexName;
var config int Steps;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var bool bDrawing;
var bool bPrimed;
var Object Stopped;
var Texture T;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (bDrawing && T != None)
	{
		if (!bPrimed)
		{
			// Set just before its first draw: its load's PostLoad has run,
			// its first Tick has not
			T.MaxFrameRate = 0.01;
			T.MinFrameRate = 0.01;
			T.PrimeCurrent = 0;
			T.PrimeCount = byte(Steps);
			bPrimed = True;
			Log("DXSINE: " $ T $ " " $ T.USize $ "x" $ T.VSize $ ", PrimeCount " $ T.PrimeCount);
		}
		C.bNoSmooth = True;
		C.Style = 1;
		C.DrawColor.R = 255;
		C.DrawColor.G = 255;
		C.DrawColor.B = 255;
		C.SetPos(64, 64);
		C.DrawTile(T, 4 * T.USize, 4 * T.VSize, 0, 0, T.USize, T.VSize);
		C.bNoSmooth = False;
	}
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

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXSINE: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXSINE: conversation " $ Stopped.Name $ " stopped");
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
				Log("DXSINE: opening 01_NYC_UNATCOIsland");
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
			T = Texture(DynamicLoadObject(TexName, class'WetTexture'));
			if (T == None)
			{
				Log("DXSINE: no " $ TexName $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
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
				Log("DXSINE: shot " $ ShotsTaken $ ", PrimeCurrent " $ T.PrimeCurrent $ ", Accumulator " $ T.Accumulator);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			Log("DXSINE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}

defaultproperties
{
	TexName="Effects.Electricity.Xplsn_EMPG"
	Steps=100
}
