//=============================================================================
// CoronaSkinConsole: a corona whose skin changes, drawn at its current frame
// (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, coronas). Liberty
// Island opened from wherever the run starts; 6 s in, the player stood where
// CoronaConsole stands, Light169 from afar, the HUD hidden, and Light169's
// Skin made the texture SkinName ([DXCapture.CoronaSkinConsole],
// CoreTexMisc.Glass.WhiteNoise_A01 by default, a chain of three noise
// frames), its MaxFrameRate and MinFrameRate Rate (0.25 by default, a frame
// every 4 s; 0 leaves the texture's own) -- Effects.Laser.LaserBeam1, a
// fire texture, for a fractal one, loaded as its own class, SkinClass
// (Engine.Texture by default; Fire.FireTexture for it). Shots 2, 6 and 10 s on, each marked for
// the original's grabber; the skin, its frames and its current frame then
// and at each shot logged; then exit. Each line is logged with "DXSKIN:" in
// front.
//=============================================================================
class CoronaSkinConsole extends Console;

var config string SkinName;
var config string SkinClass;
var config float Rate;

var float MapTime;
var float SkinTime;
var int Step;
var int MarkShot;
var string CurrentMap;
var vector StandLoc;
var rotator StandRot;
var texture Skin;

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

// Light169's skin made the texture, its chain's frames logged.
function bool SetSkin(PlayerPawn P)
{
	local Light L, Lamp;
	local texture T, F;
	local class<Object> C;
	local int n;

	foreach P.AllActors(class'Light', L)
		if (L.Name == 'Light169')
			Lamp = L;
	if (SkinName == "")
		SkinName = "CoreTexMisc.Glass.WhiteNoise_A01";
	if (SkinClass == "")
		SkinClass = "Engine.Texture";
	C = class<Object>(DynamicLoadObject(SkinClass, class'Class'));
	if (C != None)
		T = Texture(DynamicLoadObject(SkinName, C));
	if (Lamp == None || T == None)
	{
		Log("DXSKIN: no Light169 or no " $ SkinName);
		return false;
	}
	if (Rate > 0)
	{
		T.MaxFrameRate = Rate;
		T.MinFrameRate = Rate;
	}
	for (F = T; F != None && n < 16; F = F.AnimNext)
	{
		Log("DXSKIN: frame " $ n $ " " $ F);
		n++;
		if (F.AnimNext == T)
			break;
	}
	Log("DXSKIN: " $ Lamp.Name $ " skin " $ Lamp.Skin $ " made " $ T $ " (" $ T.Class $ "), " $ n $ " frames, MaxFrameRate "
		$ T.MaxFrameRate $ ", MinFrameRate " $ T.MinFrameRate $ ", scale " $ Lamp.DrawScale $ ", corona " $ Lamp.bCorona
		$ ", its current frame " $ T.AnimCurrent);
	Lamp.Skin = T;
	Skin = T;
	return true;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	SkinTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXSKIN: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
	}
	else if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;
	else if (Step == 0 && MapTime > 6.0)
	{
		StandLoc = vect(-873.028076, 5163.140625, 559.696167);
		R.Pitch = -1846;
		R.Yaw = -28672;
		R.Roll = 0;
		StandRot = R;
		P.SetLocation(StandLoc);
		P.ViewRotation = R;
		P.SetRotation(R);
		P.SetPhysics(PHYS_None);
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).ShowHud(False);
		Log("DXSKIN: standing at " $ P.Location $ " facing " $ P.ViewRotation);
		if (!SetSkin(P))
		{
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		SkinTime = 0;
		Step = 1;
	}
	else if (Step >= 1 && Step <= 3)
	{
		// Held there, as CoronaConsole holds its player; the mark up for the
		// 0.6 s before each shot.
		P.SetLocation(StandLoc);
		P.SetRotation(StandRot);
		P.ViewRotation = StandRot;
		P.Velocity = vect(0,0,0);
		P.SetPhysics(PHYS_None);
		if (SkinTime > Step * 4.0 - 2.6)
			MarkShot = Step;
		if (SkinTime > Step * 4.0 - 2.0)
		{
			Log("DXSKIN: shot " $ Step $ " " $ SkinTime $ " s after the skin, its current frame " $ Skin.AnimCurrent);
			P.ConsoleCommand("shot");
			MarkShot = 0;
			Step++;
		}
	}
	else if (Step == 4 && SkinTime > 11.0)
	{
		Log("DXSKIN: exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}

// The original's grabber keeps the frames marked as CaptureConsole marks
// them: a magenta block, then the shot's number in eight blocks.
event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (MarkShot == 0)
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

defaultproperties
{
	Rate=0.25
}
