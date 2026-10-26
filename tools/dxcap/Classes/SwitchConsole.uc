//=============================================================================
// SwitchConsole: a light switched off, and a spotlight turned, shot in both
// engines (docs/DEVELOPMENT.md, scripted runs), logged with "DXSWITCH:".
//
// On Liberty Island, 2 s in: the player held where the map starts it, on
// the pier, looking south along it and a little down, the HUD hidden,
// conversations and datalinks stopped. Then, each shot marked for the
// original's grabber: two CapLamps, steady white lights of radius 200,
// spawned 50 over the pier, the first 250 units ahead and a second 350,
// the second a moment after the first so that its change is the newer
// (shot 1); the first's LightType made LT_None, as a frobbed lamp's is
// (shot 2); the lamps destroyed and a CapSpot, a white spotlight, spawned
// where the first was, aimed south and 45 degrees down (shot 3); the
// spotlight turned east by SetRotation (shot 4); then an exit. What is
// compared between the engines: shot 2 against 1, the first lamp's pool
// gone and the second's kept; shot 4 against 3, the beam moved.
//=============================================================================
class SwitchConsole extends Console;

var float RunTime;
var int Phase;
var CapLamp Lamp, Lamp2;
var CapSpot Spot;
var vector Stand, Over;
var rotator Look;
var int MarkShot;

function Hold(PlayerPawn P)
{
	P.SetLocation(Stand);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0, 0, 0);
	P.SetPhysics(PHYS_None);
}

function Silence(PlayerPawn P)
{
	local DeusExPlayer DXP;

	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None)
		DXP.conPlay.TerminateConversation();
}

function Shoot(PlayerPawn P, int N, string What)
{
	Log("DXSWITCH: shot " $ N $ ", " $ What $ ", at " $ P.Location $ " facing " $ P.ViewRotation);
	Log("DXCAP: shot " $ N $ " at " $ P.Location $ " facing " $ P.ViewRotation);
	P.ConsoleCommand("shot");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local rotator R;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase > 0)
	{
		Silence(P);
		Hold(P);
	}
	if (Phase == 0 && RunTime > 2.0)
	{
		Stand = P.Location;
		Look.Pitch = -2500;
		Look.Yaw = 49152;
		Look.Roll = 0;
		Over = Stand + vect(0, -250, 0);
		Over.Z = Stand.Z + 50;
		Hold(P);
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).ShowHud(False);
		Log("DXSWITCH: standing at " $ P.Location $ " facing " $ P.ViewRotation $ ", the lights' spot " $ Over);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 4.0)
	{
		Lamp = P.Spawn(class'CapLamp',,, Over);
		if (Lamp == None)
		{
			Log("DXSWITCH: no lamp spawned");
			Phase = 99;
			P.ConsoleCommand("exit");
			return;
		}
		Lamp.SetPhysics(PHYS_None);
		Log("DXSWITCH: lamp at " $ Lamp.Location $ " type " $ Lamp.LightType $ " brightness " $ Lamp.LightBrightness $ " radius " $ Lamp.LightRadius);
		Phase = 11;
	}
	else if (Phase == 11 && RunTime > 4.5)
	{
		Lamp2 = P.Spawn(class'CapLamp',,, Over + vect(0, -100, 0));
		if (Lamp2 == None)
		{
			Log("DXSWITCH: no second lamp spawned");
			Phase = 99;
			P.ConsoleCommand("exit");
			return;
		}
		Lamp2.SetPhysics(PHYS_None);
		Log("DXSWITCH: second lamp at " $ Lamp2.Location);
		MarkShot = 1;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 6.5)
	{
		Shoot(P, 1, "both lamps on");
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 7.5)
	{
		Lamp.LightType = LT_None;
		Log("DXSWITCH: the first lamp switched off, type " $ Lamp.LightType);
		MarkShot = 2;
		Phase = 4;
	}
	else if (Phase == 4 && RunTime > 9.5)
	{
		Shoot(P, 2, "the first lamp off");
		Phase = 5;
	}
	else if (Phase == 5 && RunTime > 10.0)
	{
		Lamp.Destroy();
		Lamp2.Destroy();
		R.Pitch = -8192;
		R.Yaw = 49152;
		R.Roll = 0;
		Spot = P.Spawn(class'CapSpot',,, Over, R);
		if (Spot == None)
		{
			Log("DXSWITCH: no spotlight spawned");
			Phase = 99;
			P.ConsoleCommand("exit");
			return;
		}
		Spot.SetPhysics(PHYS_None);
		Spot.SetRotation(R);
		Log("DXSWITCH: spotlight at " $ Spot.Location $ " facing " $ Spot.Rotation $ " cone " $ Spot.LightCone $ " radius " $ Spot.LightRadius);
		MarkShot = 3;
		Phase = 6;
	}
	else if (Phase == 6 && RunTime > 12.0)
	{
		Shoot(P, 3, "the spotlight aimed south");
		Phase = 7;
	}
	else if (Phase == 7 && RunTime > 13.0)
	{
		R = Spot.Rotation;
		R.Yaw = 0;
		Spot.SetRotation(R);
		Log("DXSWITCH: spotlight turned, facing " $ Spot.Rotation);
		MarkShot = 4;
		Phase = 8;
	}
	else if (Phase == 8 && RunTime > 15.0)
	{
		Shoot(P, 4, "the spotlight turned east");
		MarkShot = 0;
		Phase = 9;
	}
	else if (Phase == 9 && RunTime > 17.0)
	{
		Log("DXSWITCH: done, exiting");
		Spot.Destroy();
		P.ConsoleCommand("exit");
		Phase = 99;
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
