//=============================================================================
// MatrixConsole: the Matrix cheat's sprites, shot in both engines
// (docs/DEVELOPMENT.md, scripted runs), logged with "DXMATRIX:". From
// wherever the run starts, 2 s in: the player held still looking level,
// the HUD hidden, and its Sprite set to Engine's light icon, as the Matrix
// cheat sets it to a texture every mesh and sprite is then drawn with; a
// Fire effect, a sprite, spawned 150 units ahead at eye height; a marked
// shot a moment later; then an exit. What is compared between the engines:
// the fire drawn as the icon.
//=============================================================================
class MatrixConsole extends Console;

var float RunTime;
var int Phase;
var int MarkShot;
var bool bMarking;
var vector Stand;
var rotator Look;

function Hold(PlayerPawn P)
{
	P.SetLocation(Stand);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0, 0, 0);
	P.SetPhysics(PHYS_None);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor Smoke;
	local vector Eye;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase > 0)
		Hold(P);
	if (Phase == 0 && RunTime > 2.0)
	{
		Stand = P.Location;
		Look = P.ViewRotation;
		Look.Pitch = 0;
		Look.Roll = 0;
		Hold(P);
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).ShowHud(False);
		// The player's Sprite set as the Matrix cheat sets it, to a texture
		// no sprite has (Engine's light icon)
		P.Sprite = Texture(DynamicLoadObject("Engine.S_Light", class'Texture'));
		Log("DXMATRIX: the player's Sprite set to " $ P.Sprite);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Eye = Stand;
		Eye.Z += P.BaseEyeHeight;
		Smoke = P.Spawn(class'Fire',,, Eye + vector(Look) * 150);
		if (Smoke == None)
			Log("DXMATRIX: no fire spawned");
		else
			Log("DXMATRIX: fire at " $ Smoke.Location $ ", texture " $ Smoke.Texture $ ", draw type " $ Smoke.DrawType);
		MarkShot = 1;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 3.8)
	{
		Log("DXMATRIX: shot 1, the fire");
		Log("DXCAP: shot 1");
		P.ConsoleCommand("shot");
		bMarking = false;
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 5.5)
	{
		Log("DXMATRIX: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}

// The mark for the original's screen grabber, as BorderConsole's.
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
