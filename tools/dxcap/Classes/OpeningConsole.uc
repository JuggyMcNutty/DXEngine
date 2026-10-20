//=============================================================================
// OpeningConsole: a map's opening view -- the ini's TargetMap
// ([DXCapture.OpeningConsole]), Liberty Island by default, opened from
// wherever the run starts; the player left where the map starts it, its
// view held, invulnerable, the HUD off. 4 s into the level's time, two shots
// 2 s apart, each with the mark for the original's screen grabber, the view
// logged with "DXOPEN:"; then an exit.
//=============================================================================
class OpeningConsole extends Console;

var config string TargetMap;
var float MapTime, ShotTime;
var int Step, ShotsTaken;
var string CurrentMap;
var int MarkShot;
var bool bMarking;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
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

// The mark for the original's screen grabber, as MeshConsole's.
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
			Log("DXOPEN: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 4.0)
	{
		// The original's HUD draws over the grabber's mark.
		if (DeusExRootWindow(DeusExPlayer(P).rootWindow) != None)
			DeusExRootWindow(DeusExPlayer(P).rootWindow).ShowHud(False);
		ShotTime = P.Level.TimeSeconds;
		Step = 1;
	}
	else if (Step == 1)
	{
		// A shot every 2 s, the mark shown for the 0.6 s before each.
		if (ShotsTaken < 2 && P.Level.TimeSeconds - ShotTime > 1.4 && !bMarking)
		{
			MarkShot = ShotsTaken;
			bMarking = true;
		}
		else if (bMarking && P.Level.TimeSeconds - ShotTime > 2.0)
		{
			Log("DXOPEN: shot " $ ShotsTaken $ " from " $ P.Location $ " looking " $ P.ViewRotation $ " zone " $ P.Region.Zone.Name);
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			ShotTime = P.Level.TimeSeconds;
		}
		else if (ShotsTaken >= 2 && P.Level.TimeSeconds - ShotTime > 1.0)
		{
			Log("DXOPEN: exiting");
			P.ConsoleCommand("exit");
			Step = 2;
		}
	}
}
