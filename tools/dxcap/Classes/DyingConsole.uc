//=============================================================================
// DyingConsole: the player's death and the fade after it (DeusExPlayer's
// state Dying, PlayerCalcView: up round the carcass, the view fading to
// white for 8 s, then to black for 12 s, then OPEN DXONLY) -- Liberty
// Island opened from wherever the run starts, then 6 s in the player stood
// where FlashConsole stands it and killed (KilledBy). From then, each
// second, the time since the death, the player's FlashScale and FlashFog
// and the InstantFlash and InstantFog the state sets; shots 2, 5, 8, 11, 14
// and 17 s after it, each marked for the original's grabber (0 to 5, as
// DXEngine numbers its shots). Exits 19 s after the death, before the
// game's own OPEN DXONLY. Each line is logged with "DXDYING:"; at
// DXCAP_FPS=60 the game's ViewFlash, which runs once a frame, steps as the
// original's does at 60 frames a second.
//=============================================================================
class DyingConsole extends Console;

var float MapTime;
var float StepTime;
var float DeadTime;
var float LoggedAt;
var int Phase;
var int NextShot;
var string CurrentMap;

var int MarkShot;
var bool bMarking;

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

// The console's Tick runs in the frame after the player's PlayerCalcView
// has set the flash and before it is drawn (dx-reverse-info engine-dll.md,
// Draw): these are the frame's own.
function LogFlash(PlayerPawn P, string Label)
{
	local DeusExRootWindow Root;
	local string Paused;

	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root != None)
		Paused = string(Root.bUIPaused);
	Log("DXDYING: " $ Label $ " t " $ DeadTime $ " state " $ P.GetStateName() $ " hidden " $ P.bHidden $ " scale " $ P.FlashScale $ " fog " $ P.FlashFog
		$ " instant " $ P.InstantFlash $ " " $ P.InstantFog $ " desired " $ P.DesiredFlashScale $ " " $ P.DesiredFlashFog $ " UI paused " $ Paused);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local rotator R;
	local int Shot;

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
	StepTime += Delta;

	if (Phase == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		P.ReducedDamageType = 'All';
		if (MapTime > 3.0)
		{
			Log("DXDYING: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Phase = -1;
		}
		return;
	}
	if (Phase == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Phase = 0;

	if (Phase == 0)
	{
		P.ReducedDamageType = 'All';
		if (MapTime > 6.0)
		{
			P.SetLocation(vect(-873.028076, 5163.140625, 559.696167));
			R.Pitch = -1846;
			R.Yaw = -28672;
			R.Roll = 0;
			P.SetRotation(R);
			P.ViewRotation = R;
			P.Velocity = vect(0,0,0);
			P.ReducedDamageType = '';
			LogFlash(P, "before");
			Log("DXDYING: KilledBy(None)");
			P.KilledBy(None);
			DeadTime = 0;
			LoggedAt = 0;
			NextShot = 1;
			Phase = 1;
		}
		return;
	}

	if (Phase == 1)
	{
		DeadTime += Delta;
		if (DeadTime - LoggedAt >= 1.0)
		{
			LoggedAt = DeadTime;
			LogFlash(P, "each second");
		}
		// Shot n at 3n - 1 seconds, its mark up for the 0.6 s before it.
		Shot = NextShot;
		if (Shot <= 6)
		{
			if (!bMarking && DeadTime > 3 * Shot - 1.6)
			{
				MarkShot = Shot - 1;
				bMarking = true;
			}
			else if (bMarking && DeadTime > 3 * Shot - 1.0)
			{
				LogFlash(P, "shot " $ Shot);
				P.ConsoleCommand("shot");
				bMarking = false;
				NextShot++;
			}
		}
		if (DeadTime > 19.0)
		{
			Log("DXDYING: exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
