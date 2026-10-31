//=============================================================================
// IntroDoorConsole: 00_Intro's opening as it plays, shot in both engines,
// for the trap door under Bob Page and Walton Simons
// (DeusExMover0, tag trap_door): its top, a translucent mirror of
// RockBrwnMarbl_A, lies in the plane of the marble floor round it, which is
// a translucent mirror too. TargetMap ([DXCapture.IntroDoorConsole],
// 00_Intro by default) opened from wherever the run starts, nothing touched:
// the intro's conversation runs and its cameras move the view. From First s
// of the level's time (2 by default), Shots shots (20 by default) one every
// Every s (1 by default), each marked for the original's grabber; with each
// the view's place and facing, its field of view, the view target, the
// level's time and the door's place and key, with "DXINTRODOOR:" in front. Then an exit. Run the
// original at 60 frames a second (DXCAP_FPS=60), as IntroConsole's, so that
// the cameras cut at the same level times.
//=============================================================================
class IntroDoorConsole extends Console;

var config string TargetMap;
var config float First;
var config float Every;
var config int Shots;

var string CurrentMap;
var float MapTime;
var float NextShot;
var int Step, Taken;
var int MarkShot;
var bool bMarking;

function string Target()
{
	if (TargetMap == "")
		return "00_Intro";
	return TargetMap;
}

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

function string DoorState(PlayerPawn P)
{
	local DeusExMover M;

	foreach P.AllActors(class'DeusExMover', M)
	{
		if (M.Tag == 'trap_door')
			return string(M.Name) $ " at " $ M.Location $ " key " $ M.KeyNum;
	}
	return "no trap door";
}

// The mark for the original's screen grabber, as OpeningConsole's
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

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		// The original starts in its menu map
		if (MapTime > 3.0)
		{
			Log("DXINTRODOOR: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1)
	{
		if (CurrentMap ~= Target())
			Step = 0;
		return;
	}

	if (Step == 0)
	{
		if (First == 0)
			First = 2;
		if (Every == 0)
			Every = 1;
		if (Shots == 0)
			Shots = 20;
		NextShot = First;
		Log("DXINTRODOOR: " $ DoorState(P));
		Step = 1;
	}
	// The HUD off before each mark: the original's draws over it
	if (Step == 1 && P.Level.TimeSeconds > NextShot - 0.6)
	{
		if (DeusExPlayer(P) != None && DeusExRootWindow(DeusExPlayer(P).rootWindow) != None)
			DeusExRootWindow(DeusExPlayer(P).rootWindow).ShowHud(False);
		MarkShot = Taken;
		bMarking = true;
		Step = 2;
	}
	else if (Step == 2 && P.Level.TimeSeconds > NextShot)
	{
		Log("DXINTRODOOR: shot " $ Taken $ " at " $ P.Level.TimeSeconds $ ": the view at " $ P.Location $ " facing " $ P.ViewRotation
			$ ", field of view " $ P.FOVAngle $ " (desired " $ P.DesiredFOV $ "), view target " $ P.ViewTarget $ ", " $ DoorState(P));
		Log("DXCAP: shot " $ Taken);
		P.ConsoleCommand("shot");
		bMarking = false;
		Taken++;
		NextShot += Every;
		if (Taken < Shots)
			Step = 1;
		else
			Step = 3;
	}
	else if (Step == 3 && P.Level.TimeSeconds > NextShot)
	{
		Log("DXINTRODOOR: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
	}
}
