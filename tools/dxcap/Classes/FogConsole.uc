//=============================================================================
// FogConsole: meshes in a fog zone, shot in both engines (docs/DEVELOPMENT.md,
// scripted runs). The ini's TargetMap ([DXCapture.FogConsole],
// 02_NYC_Underground by default, whose start is in a fog zone) opened from
// wherever the run starts; the player left where the map starts it, its
// view held, invulnerable, the HUD off. 4 s into the level's time a
// UNATCOTroop, standing still, and a large crate are spawned 220 units
// ahead in the view, 60 units either side of its axis; each one's place and
// zone logged with "DXFOG:"; then two shots 2 s apart, each with the mark
// for the original's screen grabber; then an exit. What is compared between
// the engines: the fog on the two meshes against the fog on the floor and
// walls round them.
//=============================================================================
class FogConsole extends Console;

var config string TargetMap;
var float MapTime, ShotTime;
var int Step, ShotsTaken;
var string CurrentMap;
var int MarkShot;
var bool bMarking;
var vector Stand;
var rotator Look;
var Actor Troop, Crate;

function string Target()
{
	if (TargetMap == "")
		return "02_NYC_Underground";
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

function string ZoneOf(Actor A)
{
	if (A.Region.Zone == None)
		return "none";
	if (A.Region.Zone.bFogZone)
		return string(A.Region.Zone.Name) $ " (fog)";
	return string(A.Region.Zone.Name);
}

function Hold(PlayerPawn P)
{
	P.SetLocation(Stand);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0, 0, 0);
	P.SetPhysics(PHYS_None);
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

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector Ahead, Side;
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
			Log("DXFOG: opening " $ Target());
			P.ConsoleCommand("open " $ Target());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= Target()))
		Step = 0;
	if (Step >= 1)
		Hold(P);

	if (Step == 0 && P.Level.TimeSeconds > 4.0)
	{
		if (DeusExRootWindow(DeusExPlayer(P).rootWindow) != None)
			DeusExRootWindow(DeusExPlayer(P).rootWindow).ShowHud(False);
		Stand = P.Location;
		Look = P.ViewRotation;
		Look.Pitch = 0;
		Look.Roll = 0;
		R = Look;
		Ahead = vector(R);
		R.Yaw += 16384;
		Side = vector(R);
		Log("DXFOG: standing at " $ Stand $ " looking " $ Look $ " zone " $ ZoneOf(P));
		Troop = P.Spawn(class'UNATCOTroop',,, Stand + Ahead * 220 - Side * 60);
		if (Troop != None)
		{
			Troop.SetPhysics(PHYS_Falling);
			Log("DXFOG: troop at " $ Troop.Location $ " zone " $ ZoneOf(Troop));
		}
		else
			Log("DXFOG: no troop spawned");
		Crate = P.Spawn(class'CrateUnbreakableLarge',,, Stand + Ahead * 220 + Side * 60);
		if (Crate != None)
		{
			Crate.SetPhysics(PHYS_Falling);
			Log("DXFOG: crate at " $ Crate.Location $ " zone " $ ZoneOf(Crate));
		}
		else
			Log("DXFOG: no crate spawned");
		ShotTime = P.Level.TimeSeconds;
		Step = 1;
	}
	else if (Step == 1)
	{
		// A shot every 2 s, the mark shown for the 0.6 s before each.
		if (ShotsTaken < 2 && P.Level.TimeSeconds - ShotTime > 1.4 && !bMarking)
		{
			MarkShot = ShotsTaken + 1;
			bMarking = true;
		}
		else if (bMarking && P.Level.TimeSeconds - ShotTime > 2.0)
		{
			Log("DXFOG: shot " $ (ShotsTaken + 1) $ " from " $ P.Location $ " looking " $ P.ViewRotation);
			Log("DXCAP: shot " $ (ShotsTaken + 1) $ " at " $ P.Location $ " facing " $ P.ViewRotation);
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			ShotTime = P.Level.TimeSeconds;
		}
		else if (ShotsTaken >= 2 && P.Level.TimeSeconds - ShotTime > 1.0)
		{
			Log("DXFOG: exiting");
			P.ConsoleCommand("exit");
			Step = 2;
		}
	}
}
