//=============================================================================
// FogTintConsole: a volumetric light's fog over a fog zone's surfaces, and
// whether it follows the light (docs/DEVELOPMENT.md, scripted runs;
// docs/NATIVES.md, lighting). TargetMap ([DXCapture.FogTintConsole],
// 02_NYC_Bar by default) opened from wherever the run starts; 6 s in, the
// HUD hidden, conversations and datalinks stopped as they start, the light
// LightName (Light90, VolumeRadius 128: a sphere 3,225 units across the
// bar's back room) given LightBrightness PreBrightness first when that is
// not 0, then the player, out of the world and with no shadow, stood at the
// zone ZoneName's own place (ZoneInfo5, a fog zone 152 units from the
// light), looking along Yaw (32768 by default, away from the light, past
// the 325 units its light map reaches) and level. Shots 3 s apart, each
// marked for the original's grabber: as it stands; then with the light's
// LightBrightness 255; then with its LightHue 0 and LightSaturation 0, a
// pure red. The light's settings, the zone's bFogZone and the player's zone
// logged at each; then exit. With TriggerTag set, the light's settings are
// left alone and every actor of that Tag is triggered after the first shot
// instead, the shots 5 s apart: on 04_NYC_NSFHQ (LightName TriggerLight0,
// ZoneName ZoneInfo1, TriggerTag GasGoesOff) the gas room's six
// TriggerLights, VolumeBrightness 220 and VolumeFog 100, toggled off as
// Switch0 or Switch1 toggles them in play. With Baseline, a shot before
// those with every volumetric light's VolumeBrightness 0, which leaves its
// fog no density and its surfaces their fog maps, clear, in either engine,
// then each put back: each shot after it less it is the fog alone
// (fogdelta.py). Each line is logged with "DXFOG:" in front.
// Render.dll makes the fog map each frame from the light as it is then
// (URender::GlobalLighting 0x10b05e90; FLightManager::SetupLight
// 0x10b060b3-0x10b06c19), as DXEngine does.
//=============================================================================
class FogTintConsole extends Console;

var config string TargetMap;
var config string LightName;
var config string ZoneName;
var config int Yaw;
var config byte PreBrightness;
var config string TriggerTag;
var config bool Baseline;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Object Stopped;
var Actor Lamp;
var ZoneInfo Zone;
var vector Eye;
var rotator View;

var int MarkShot;
var bool bMarking;
var int ShotStep;
var Actor Dimmed[32];
var byte DimmedBrightness[32];
var int NumDimmed;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function LogLamp(PlayerPawn P, string Tag)
{
	Log("DXFOG: " $ Tag $ ": " $ Lamp.Name $ " type " $ Lamp.LightType $ " effect " $ Lamp.LightEffect
		$ " brightness " $ Lamp.LightBrightness $ " hue " $ Lamp.LightHue $ " saturation " $ Lamp.LightSaturation
		$ " radius " $ Lamp.LightRadius $ " volume radius " $ Lamp.VolumeRadius $ " brightness " $ Lamp.VolumeBrightness
		$ " fog " $ Lamp.VolumeFog $ "; the player in " $ P.Region.Zone $ " (fog zone " $ P.Region.Zone.bFogZone
		$ ") at " $ P.Location $ " view " $ P.ViewRotation);
}

function float Gap()
{
	if (TriggerTag != "")
		return 5.0;
	return 3.0;
}

// The baseline's shot, then the three
function int Shots()
{
	if (Baseline)
		return 4;
	return 3;
}

// Every volumetric light's fog off for the baseline, or back
function DimFog(PlayerPawn P, bool bOff)
{
	local Actor A;
	local int i;

	if (bOff)
	{
		foreach P.AllActors(class'Actor', A)
		{
			if (A.VolumeRadius != 0 && A.VolumeBrightness != 0 && NumDimmed < 32)
			{
				Dimmed[NumDimmed] = A;
				DimmedBrightness[NumDimmed] = A.VolumeBrightness;
				A.VolumeBrightness = 0;
				NumDimmed++;
			}
		}
		Log("DXFOG: " $ NumDimmed $ " volumetric lights' VolumeBrightness 0");
	}
	else
	{
		for (i = 0; i < NumDimmed; i++)
			if (Dimmed[i] != None)
				Dimmed[i].VolumeBrightness = DimmedBrightness[i];
		Log("DXFOG: " $ NumDimmed $ " volumetric lights' VolumeBrightness back");
	}
}

function HoldView(PlayerPawn P)
{
	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.ViewRotation = View;
	P.SetRotation(View);
}

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
		Log("DXFOG: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXFOG: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local Actor A;
	local ZoneInfo Z;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None && MapOf(P) ~= TargetMap)
		Silence(DXP);

	if (Phase == 0)
	{
		if (!(MapOf(P) ~= TargetMap))
		{
			if (MapTime > 3.0)
			{
				Log("DXFOG: opening " $ TargetMap);
				P.ConsoleCommand("open " $ TargetMap);
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
		if (MapOf(P) ~= TargetMap && MapTime > 6.0 && DXP != None)
		{
			foreach P.AllActors(class'Actor', A)
				if (string(A.Name) ~= LightName)
					Lamp = A;
			foreach P.AllActors(class'ZoneInfo', Z)
				if (string(Z.Name) ~= ZoneName)
					Zone = Z;
			if (Lamp == None || Zone == None)
			{
				Log("DXFOG: no " $ LightName $ " or " $ ZoneName $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			if (PreBrightness != 0)
				Lamp.LightBrightness = PreBrightness;
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			DXP.KillShadow();
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			Eye = Zone.Location;
			View.Pitch = 0;
			View.Yaw = Yaw;
			View.Roll = 0;
			HoldView(P);
			if (Baseline)
				DimFog(P, true);
			LogLamp(P, "placed, zone " $ Zone.Name $ " fog zone " $ Zone.bFogZone);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		HoldView(P);
		if (StepTime > Gap() && ShotsTaken < Shots())
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = Gap() - 0.6;
			}
			else
			{
				LogLamp(P, "shot " $ ShotsTaken);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
				ShotStep = ShotsTaken;
				if (Baseline)
					ShotStep--;
				if (Baseline && ShotStep == 0)
					DimFog(P, false);
				else if (TriggerTag != "")
				{
					if (ShotStep == 1)
					{
						foreach P.AllActors(class'Actor', A)
							if (string(A.Tag) ~= TriggerTag)
							{
								Log("DXFOG: triggering " $ A.Name $ " in state " $ A.GetStateName());
								A.Trigger(P, P);
							}
					}
				}
				else if (ShotStep == 1)
					Lamp.LightBrightness = 255;
				else if (ShotStep == 2)
				{
					Lamp.LightHue = 0;
					Lamp.LightSaturation = 0;
				}
			}
		}
		else if (ShotsTaken >= Shots() && StepTime > 2.0)
		{
			Log("DXFOG: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}

defaultproperties
{
	TargetMap="02_NYC_Bar"
	LightName="Light90"
	ZoneName="ZoneInfo5"
	Yaw=32768
	PreBrightness=0
	TriggerTag=""
	Baseline=False
}
