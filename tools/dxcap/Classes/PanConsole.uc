//=============================================================================
// PanConsole: auto-panned surfaces held still at a known time -- the sky's
// cloud banks (ClenCloudBank_A and _B, their zone's TexUPanSpeed) over the
// ini's TargetMap ([DXCapture.PanConsole]), Liberty Island by default,
// opened from wherever the run starts; the player left where the map starts
// him, the HUD off, his view turned Pitch up (8000 by default). Shots in the
// level's time at ShotA and ShotB (6 and 8 s by default): at each the level's
// TimeDilation made 0.0025, so its TimeSeconds, which the pan goes by in the
// original's Render.dll, all but holds -- a pause does not hold it, in
// either engine --; 0.4 s of the wall clock on the mark for the original's
// screen grabber, and 1 s on the shot, the time logged; then the level let
// go. An exit after the second. Lines "DXPAN:".
//=============================================================================
class PanConsole extends Console;

var config string TargetMap;
var config int Pitch;
var config float ShotA, ShotB;
var float MapTime;
var int HeldSince;
var int Step, ShotsTaken;
var string CurrentMap;
var int MarkShot;
var bool bMarking, bHeld;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function float ShotAt(int n)
{
	if (n == 0)
	{
		if (ShotA > 0)
			return ShotA;
		return 6.0;
	}
	if (ShotB > 0)
		return ShotB;
	return 8.0;
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

// The mark for the original's screen grabber, as OpeningConsole's.
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

function int ViewPitch()
{
	if (Pitch != 0)
		return Pitch;
	return 8000;
}

// The wall clock in ms within the hour, from the level's local time, which
// both engines take from the system's clock each frame.
function int WallMs(PlayerPawn P)
{
	return (P.Level.Minute * 60 + P.Level.Second) * 1000 + P.Level.Millisecond;
}

function int HeldMs(PlayerPawn P)
{
	local int d;

	d = WallMs(P) - HeldSince;
	if (d < 0)
		d += 3600000;
	return d;
}

function LogZones(PlayerPawn P)
{
	local ZoneInfo Z;

	foreach P.AllActors(class'ZoneInfo', Z)
		if (Z.TexUPanSpeed != 1.0 || Z.TexVPanSpeed != 1.0 || SkyZoneInfo(Z) != None)
			Log("DXPAN: zone " $ Z.Name $ " sky " $ (SkyZoneInfo(Z) != None) $ " pan " $ Z.TexUPanSpeed $ " " $ Z.TexVPanSpeed);
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
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= Target()))
	{
		if (MapTime > 3.0)
		{
			Log("DXPAN: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 3.0)
	{
		// The original's HUD draws over the grabber's mark.
		if (DeusExRootWindow(DeusExPlayer(P).rootWindow) != None)
			DeusExRootWindow(DeusExPlayer(P).rootWindow).ShowHud(False);
		R = P.ViewRotation;
		R.Pitch = ViewPitch();
		P.ViewRotation = R;
		LogZones(P);
		Step = 1;
	}
	else if (Step == 1)
	{
		R = P.ViewRotation;
		R.Pitch = ViewPitch();
		P.ViewRotation = R;
		if (!bHeld && ShotsTaken < 2 && P.Level.TimeSeconds >= ShotAt(ShotsTaken))
		{
			P.Level.TimeDilation = 0.0025;
			bHeld = true;
			HeldSince = WallMs(P);
			Log("DXPAN: held at " $ P.Level.TimeSeconds $ " for shot " $ ShotsTaken);
		}
		else if (bHeld)
		{
			if (HeldMs(P) > 400 && !bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
			}
			else if (bMarking && HeldMs(P) > 1000)
			{
				Log("DXPAN: shot " $ ShotsTaken $ " at " $ P.Level.TimeSeconds $ " from " $ P.Location $ " looking " $ P.ViewRotation);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				bHeld = false;
				P.Level.TimeDilation = 1.0;
			}
		}
		else if (ShotsTaken >= 2)
		{
			Log("DXPAN: exiting");
			P.ConsoleCommand("exit");
			Step = 2;
		}
	}
}
