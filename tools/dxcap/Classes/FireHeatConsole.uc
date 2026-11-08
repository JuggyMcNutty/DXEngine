//=============================================================================
// FireHeatConsole: fire textures whose RenderHeat the package leaves at its
// default of 0 (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, fire,
// water and ice textures). Liberty Island opened from wherever the run
// starts; 6 s in, the HUD hidden, four fires drawn on the canvas every
// frame over a grey box, each 192 by 192, unsmoothed, STY_Normal:
// Effects.Fire.SparkFX1 (a damaged robot's sparks, DeusEx.u's
// SpawnEMPSparks), Effects.Fire.Spark_Electric (the maps' sparking
// ParticleGenerators), Effects.Electricity.Wepn_EMPG_SFX (the EMP grenade's
// and the riot prod's electricity), none of them with a RenderHeat, and
// Effects.Fire.FireballWhite (RenderHeat 220) for the control. After the
// first draw each one's RenderHeat, OldRenderHeat and colour table entries
// 0, 512 and 1020 are logged: Fire.dll's PostLoad (0x10507b70) builds the
// table for any RenderHeat from an OldRenderHeat its constructor sets to -1
// (0x10507840), sum/4 + 1 - (255 - RenderHeat)/16 held to 0..255, so 0, 113
// and 240 for RenderHeat 0 and 0, 126 and 253 for 220. Two shots 2 s apart,
// each marked for the original's grabber; then exit. Each line is logged
// with "DXHEAT:" in front.
//=============================================================================
class FireHeatConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var bool bDrawing;
var bool bLogged;
var int Drawn;
var Object Stopped;
var FireTexture Fires[4];
var string FireNames[4];

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function LoadFires()
{
	local int i;

	for (i = 0; i < 4; i++)
	{
		Fires[i] = FireTexture(DynamicLoadObject(FireNames[i], class'FireTexture'));
		Log("DXHEAT: " $ FireNames[i] $ " " $ Fires[i]);
	}
}

function LogFires()
{
	local int i;

	for (i = 0; i < 4; i++)
	{
		if (Fires[i] == None)
			continue;
		Log("DXHEAT: " $ Fires[i].Name $ " RenderHeat " $ Fires[i].RenderHeat
			$ " OldRenderHeat " $ Fires[i].OldRenderHeat
			$ " sparks " $ Fires[i].NumSparks);
		// RenderTable (1,028 bytes) is past what UCC reads through another
		// object's context (255): the shots show its colours.
	}
}

function DrawFires(canvas C)
{
	local int i;

	C.bNoSmooth = True;
	C.Style = 1;
	C.DrawColor.R = 128;
	C.DrawColor.G = 128;
	C.DrawColor.B = 128;
	C.SetPos(16, 40);
	C.DrawRect(Texture'Solid', 856, 240);
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;
	for (i = 0; i < 4; i++)
	{
		if (Fires[i] == None)
			continue;
		C.SetPos(40 + 208 * i, 64);
		C.DrawTile(Fires[i], 192, 192, 0, 0, Fires[i].USize, Fires[i].VSize);
	}
	C.bNoSmooth = False;
}

event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (bDrawing)
	{
		DrawFires(C);
		Drawn++;
		if (Drawn == 2 && !bLogged)
		{
			bLogged = True;
			LogFires();
		}
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

// A conversation's letterbox or a datalink's window would cover the tiles.
function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXHEAT: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXHEAT: conversation " $ Stopped.Name $ " stopped");
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
				Log("DXHEAT: opening 01_NYC_UNATCOIsland");
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
			LoadFires();
			bDrawing = true;
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
				Log("DXHEAT: shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			LogFires();
			Log("DXHEAT: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}

defaultproperties
{
	FireNames(0)="Effects.Fire.SparkFX1"
	FireNames(1)="Effects.Fire.Spark_Electric"
	FireNames(2)="Effects.Electricity.Wepn_EMPG_SFX"
	FireNames(3)="Effects.Fire.FireballWhite"
}
