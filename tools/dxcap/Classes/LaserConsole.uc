//=============================================================================
// LaserConsole: a laser fence's beams, looked into -- Liberty Island opened
// from wherever the run starts, then 6 s in the player stood on the floor
// below where CaptureConsole shoots the first LaserTrigger from, the top one
// of a fence of five, the lower three in view, the HUD hidden; each second
// the player's place and view, and the trigger's, its emitter's and the
// emitter's proxy's state logged (on, hidden, frozen, draw type and style,
// textures, render interface, the spot; the proxy's mesh and lighting);
// shots 2 s and 4 s after standing, a third 6 s after with the proxy's skin
// drawn large on the canvas over black, plain and translucent, so the
// texture is seen apart from the beam; then every emitter's proxy given a
// still texture (Effects' Gen_Red) in each of its slots, so that the beams
// draw alike in both engines where the fire's sparks differ from run to run:
// a fourth shot 8 s after standing with the proxies unlit, their mesh apart
// from its lighting, and a fifth 10 s after with them lit -- each marked for
// the original's grabber, numbered from 0 as DXEngine names its shots, the
// mark shown once its shot's change is made; exits 2 s later. With NoDetail
// ([DXCapture.LaserConsole]) the detail textures are turned off first, as
// the Display menu turns them off.
//=============================================================================
class LaserConsole extends Console;

var config bool NoDetail;
var float MapTime, LogTime;
var int Step;
var bool bChanged;
var LaserTrigger LT;
var string CurrentMap;

// The map of the level's URL: past the last '/', before '.' or '?'.
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

function LogLaser()
{
	local LaserEmitter E;

	if (LT == None)
	{
		Log("DXLASER: no LaserTrigger");
		return;
	}
	E = LT.emitter;
	Log("DXLASER: trigger " $ string(LT) $ " on " $ LT.bIsOn $ " at " $ LT.Location $ " turned " $ LT.Rotation $ " emitter " $ string(E));
	if (E == None)
		return;
	Log("DXLASER: emitter on " $ E.bIsOn $ " hidden " $ E.bHidden $ " frozen " $ E.bFrozen $ " hiddenbeam " $ E.bHiddenBeam $ " drawtype " $ E.DrawType $ " style " $ E.Style $ " texture " $ string(E.Texture) $ " skin " $ string(E.Skin) $ " iterator class " $ string(E.RenderIteratorClass) $ " interface " $ string(E.RenderInterface) $ " spot " $ string(E.spot[0]));
	if (E.proxy != None)
	{
		Log("DXLASER: proxy " $ string(E.proxy) $ " hidden " $ E.proxy.bHidden $ " drawtype " $ E.proxy.DrawType $ " style " $ E.proxy.Style $ " texture " $ string(E.proxy.Texture) $ " skin " $ string(E.proxy.Skin) $ " scale " $ E.proxy.DrawScale $ " at " $ E.proxy.Location $ " distance " $ E.proxy.DistanceFromPlayer $ " iterator class " $ string(E.proxy.RenderIteratorClass));
		Log("DXLASER: proxy look mesh " $ string(E.proxy.Mesh) $ " rotation " $ E.proxy.Rotation $ " glow " $ E.proxy.ScaleGlow $ " ambient " $ E.proxy.AmbientGlow $ " unlit " $ E.proxy.bUnlit $ " fatness " $ E.proxy.Fatness $ " lighttype " $ E.proxy.LightType);
		if (E.proxy.Skin != None)
			Log("DXLASER: proxy skin " $ string(E.proxy.Skin) $ " size " $ E.proxy.Skin.USize $ "x" $ E.proxy.Skin.VSize $ " maxcolor " $ E.proxy.Skin.MaxColor.R $ "," $ E.proxy.Skin.MaxColor.G $ "," $ E.proxy.Skin.MaxColor.B);
	}
	if (E.spot[0] != None)
		Log("DXLASER: spot at " $ E.spot[0].Location $ " hidden " $ E.spot[0].bHidden $ " drawtype " $ E.spot[0].DrawType $ " texture " $ string(E.spot[0].Texture));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local LaserEmitter E;
	local Texture T;
	local int i;

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

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXLASER: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
	}
	else if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;
	else if (Step == 0 && MapTime > 6.0)
	{
		foreach P.AllActors(class'LaserTrigger', LT)
			break;
		// On the floor, where a player dropped there comes to rest, which a
		// fall leaves a fraction of a unit apart from run to run and the thin
		// beams a row or two; looking down as at the trigger's beam from 70
		// units in front of it and 42.8 above
		P.SetLocation(vect(1920.30, -878.64, -96.20));
		P.ViewRotation = rot(59813, 49152, 0);
		P.SetRotation(P.ViewRotation);
		Log("DXLASER: standing at " $ P.Location $ " looking " $ P.ViewRotation);
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).ShowHud(False);
		if (NoDetail)
		{
			P.ConsoleCommand("set ini:Engine.Engine.GameRenderDevice DetailTextures False");
			Log("DXLASER: detail textures " $ P.ConsoleCommand("get ini:Engine.Engine.GameRenderDevice DetailTextures"));
		}
		Step = 1;
	}
	if (Step >= 1 && MapTime - LogTime >= 1.0)
	{
		LogTime = MapTime;
		Log("DXLASER: player at " $ P.Location $ " view " $ P.ViewRotation);
		LogLaser();
	}
	// A change half a second after a shot, never in its tick, to every
	// emitter's proxy
	if (Step == 4 && MapTime > 12.5 && !bChanged)
	{
		// In every slot: a slot takes its MultiSkins entry before the mesh's
		// own texture, the Skin only after it
		T = Texture(DynamicLoadObject("Effects.Generated.Gen_Red", class'Texture'));
		foreach P.AllActors(class'LaserEmitter', E)
		{
			if (E.proxy == None)
				continue;
			E.proxy.bUnlit = True;
			E.proxy.Skin = T;
			for (i = 0; i < ArrayCount(E.proxy.MultiSkins); i++)
				E.proxy.MultiSkins[i] = T;
		}
		bChanged = True;
		Log("DXLASER: proxies unlit, " $ string(T) $ " in every slot");
	}
	else if (Step == 5 && MapTime > 14.5 && !bChanged)
	{
		foreach P.AllActors(class'LaserEmitter', E)
			if (E.proxy != None)
				E.proxy.bUnlit = False;
		bChanged = True;
		Log("DXLASER: proxies lit");
	}
	if ((Step == 1 && MapTime > 8.0) || (Step == 2 && MapTime > 10.0) || (Step == 3 && MapTime > 12.0) || (Step == 4 && MapTime > 14.0) || (Step == 5 && MapTime > 16.0))
	{
		Log("DXCAP: shot " $ (Step - 1));
		P.ConsoleCommand("shot");
		Step++;
		bChanged = False;
	}
	else if (Step == 6 && MapTime > 18.0)
	{
		Log("DXLASER: exiting");
		P.ConsoleCommand("exit");
		Step = 7;
	}
}

// The proxy's skin, 8 times its size, over black: plain at the top, then
// translucent, as the beam draws it.
function DrawSwatch(canvas C, Texture T)
{
	C.DrawColor.R = 0;
	C.DrawColor.G = 0;
	C.DrawColor.B = 0;
	C.Style = 1;
	C.SetPos(96, 96);
	C.DrawRect(Texture'Solid', T.USize * 8 + 32, T.VSize * 16 + 48);
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;
	C.SetPos(112, 112);
	C.DrawTile(T, T.USize * 8, T.VSize * 8, 0, 0, T.USize, T.VSize);
	C.Style = 3;
	C.SetPos(112, 128 + T.VSize * 8);
	C.DrawTile(T, T.USize * 8, T.VSize * 8, 0, 0, T.USize, T.VSize);
}

// The original's grabber keeps the frames marked as CaptureConsole marks
// them: a magenta block, then the shot's number, Step - 1, in eight blocks.
event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (Step < 1 || Step > 5 || (Step >= 4 && !bChanged))
		return;
	if (Step == 3 && LT != None && LT.emitter != None && LT.emitter.proxy != None && LT.emitter.proxy.Skin != None)
		DrawSwatch(C, LT.emitter.proxy.Skin);
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if (((Step - 1) & (1 << i)) != 0)
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
