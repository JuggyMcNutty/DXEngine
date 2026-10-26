//=============================================================================
// DrugConsole: the drugged view (DeusExPlayer.DrugEffects) -- Liberty Island
// opened from wherever the run starts, then 6 s in the player stood where
// FlashConsole stands it, the HUD up with each of its windows hidden but
// itself, so only its background draws; a shot some 4 s later with no
// drug, then drugEffectTimer set to 10, as a tranquilizer dart's
// StartPoison adds 4, and shots some 2 s and 4 s into it: the HUD's
// background the WetTexture DrunkFX, smoothed, stretched and modulated over
// the view. Each shot is marked for the original's grabber for 0.6 s
// before it, numbered from 0 as DXEngine's shots are; the drug's timer and
// the view's angle are logged with each. Exits 2 s after the last. Each
// line is logged with "DXDRUG:". With HudWindows the HUD's windows stay
// shown, drawn over the drugged view as in play.
//=============================================================================
class DrugConsole extends Console;

var config bool HudWindows;

var float MapTime;
var float StepTime;
var int Phase;
var string CurrentMap;
var vector StandLoc;
var rotator StandRot;

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

// Marked for 0.6 s, then shot.
function bool Shoot(PlayerPawn P, int Shot)
{
	if (!bMarking)
	{
		MarkShot = Shot;
		bMarking = true;
		StepTime = 0;
		return false;
	}
	if (StepTime < 0.6)
		return false;
	Log("DXDRUG: shot " $ Shot $ " drug " $ DeusExPlayer(P).drugEffectTimer $ " fov " $ P.FovAngle $ " desired " $ P.DesiredFOV);
	P.ConsoleCommand("shot");
	bMarking = false;
	StepTime = 0;
	return true;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow Root;
	local Window W;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP == None)
		return;

	if (Phase == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXDRUG: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Phase = -1;
		}
		return;
	}
	if (Phase == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Phase = 0;

	if (Phase == 0 && MapTime > 6.0)
	{
		StandLoc = vect(-873.028076, 5163.140625, 559.696167);
		R.Pitch = -1846;
		R.Yaw = -28672;
		R.Roll = 0;
		StandRot = R;
		Root = DeusExRootWindow(DXP.rootWindow);
		Root.ShowHud(True);
		W = Root.hud.GetBottomChild();
		while (W != None && !HudWindows)
		{
			W.Hide();
			W = W.GetHigherSibling();
		}
		Log("DXDRUG: standing at " $ StandLoc $ " facing " $ StandRot $ " windows " $ HudWindows);
		StepTime = 0;
		Phase = 1;
	}
	if (Phase >= 1 && Phase < 9)
	{
		P.SetLocation(StandLoc);
		P.Velocity = vect(0,0,0);
		P.SetPhysics(PHYS_None);
	}
	if (Phase == 1 && StepTime > 2.0)
	{
		P.SetRotation(StandRot);
		P.ViewRotation = StandRot;
		if (Shoot(P, 0))
		{
			DXP.drugEffectTimer = 10.0;
			Log("DXDRUG: drugEffectTimer 10");
			Phase = 2;
		}
	}
	else if (Phase == 2 && StepTime > 0.9)
	{
		if (Shoot(P, 1))
			Phase = 3;
	}
	else if (Phase == 3 && StepTime > 0.9)
	{
		if (Shoot(P, 2))
			Phase = 4;
	}
	else if (Phase == 4 && StepTime > 2.0)
	{
		Log("DXDRUG: exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
