//=============================================================================
// SpriteConsole: a sprite's style, the same in both engines
// (docs/DEVELOPMENT.md, scripted runs). At Liberty Island's start, level and
// facing the start's way, the HUD hidden: four sprites of one icon 200 units
// ahead, left to right STY_Normal, STY_Masked, STY_Translucent and
// STY_Modulated -- the icon a texture not itself masked, its colour 0
// magenta -- and above them, 280 ahead, a generator with neither
// bTranslucent nor bModulated, whose particles (that icon, rising) its
// proxy draws STY_Masked; then two shots 2 s apart, each marked for the
// original's grabber; then exit. Styles ([DXCapture.SpriteConsole], "1234"
// by default) places only the sprites of the styles it names, each in its
// own place; NoParticles leaves the generator out. The map's own generators
// and the stock one's defaults are logged first. Each line is logged with
// "DXSPRITE:" in front.
//=============================================================================
class SpriteConsole extends Console;

var config string Styles;
var config bool NoParticles;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var CapSprite Placed[4];
var ParticleGenerator Gen;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

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

function CapSprite Place(PlayerPawn P, vector Where, int S)
{
	local CapSprite A;
	A = P.Spawn(class'CapSprite',,, Where);
	if (A == None)
	{
		Log("DXSPRITE: no room for a sprite at " $ Where);
		return None;
	}
	A.Texture = Texture'DeusExUI.Icons.LargeIconBioCell';
	if (S == 1)
		A.Style = STY_Normal;
	else if (S == 2)
		A.Style = STY_Masked;
	else if (S == 3)
		A.Style = STY_Translucent;
	else
		A.Style = STY_Modulated;
	A.DrawScale = 1.0;
	Log("DXSPRITE: " $ A.Name $ " at " $ A.Location $ " style " $ int(A.Style) $ " texture " $ A.Texture
		$ " masked " $ A.Texture.bMasked $ " size " $ A.Texture.USize $ "x" $ A.Texture.VSize $ " scale " $ A.DrawScale
		$ " glow " $ A.ScaleGlow $ " smooth off " $ A.bNoSmooth);
	return A;
}

function ParticleGenerator Spew(PlayerPawn P, vector Where)
{
	local ParticleGenerator G;
	local rotator R;

	R.Pitch = 16384;
	G = P.Spawn(class'ParticleGenerator',,, Where, R);
	if (G == None)
	{
		Log("DXSPRITE: no room for a generator at " $ Where);
		return None;
	}
	G.frequency = 1.0;
	G.checkTime = 0.25;
	G.numPerSpawn = 1;
	G.riseRate = 20;
	G.ejectSpeed = 0;
	G.particleLifeSpan = 3.0;
	G.particleDrawScale = 0.5;
	G.bScale = false;
	G.bFade = false;
	G.bGravity = false;
	G.bRandomEject = false;
	G.bTranslucent = false;
	G.bModulated = false;
	G.bParticlesUnlit = true;
	G.particleTexture = Texture'DeusExUI.Icons.LargeIconBioCell';
	G.bTriggered = false;
	G.bSpewing = true;
	if (G.proxy != None)
	{
		G.proxy.bHidden = false;
		G.SetProxyData();
	}
	Log("DXSPRITE: " $ G.Name $ " at " $ G.Location $ " proxy " $ G.proxy $ " texture " $ G.proxy.Texture
		$ " style " $ int(G.proxy.Style) $ " draw type " $ int(G.proxy.DrawType));
	return G;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local ParticleGenerator G;
	local vector X, Y, Z, Eye;
	local rotator R;
	local int i;

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
				Log("DXSPRITE: opening 01_NYC_UNATCOIsland");
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
			Log("DXSPRITE: the stock generator: translucent " $ class'ParticleGenerator'.default.bTranslucent
				$ " modulated " $ class'ParticleGenerator'.default.bModulated $ " texture " $ class'ParticleGenerator'.default.particleTexture);
			foreach P.AllActors(class'ParticleGenerator', G)
				Log("DXSPRITE: the map's " $ G.Name $ " translucent " $ G.bTranslucent $ " modulated " $ G.bModulated
					$ " texture " $ G.particleTexture $ " proxy style " $ int(G.proxy.Style));
			R = P.ViewRotation;
			R.Pitch = 0;
			P.ViewRotation = R;
			P.SetRotation(R);
			GetAxes(R, X, Y, Z);
			Eye = P.Location + Z * P.EyeHeight;
			Log("DXSPRITE: the player at " $ P.Location $ " rotation " $ P.ViewRotation $ " eye height " $ P.EyeHeight);
			if (Styles == "")
				Styles = "1234";
			for (i = 0; i < 4; i++)
				if (InStr(Styles, string(i + 1)) >= 0)
					Placed[i] = Place(P, Eye + X * 200 + Y * (70 * i - 105) - Z * 20, i + 1);
			if (!NoParticles)
				Gen = Spew(P, Eye + X * 280 + Z * 30);
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// The particles risen after 4 s; a shot every 2 s from then, the mark
		// shown for the 0.6 s before each.
		if (StepTime > 4.0 && ShotsTaken < 2)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 3.4;
			}
			else
			{
				Log("DXSPRITE: shot " $ ShotsTaken $ " at " $ P.Level.TimeSeconds);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 2.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			Log("DXSPRITE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
