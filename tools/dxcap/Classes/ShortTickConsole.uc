//=============================================================================
// ShortTickConsole (bugs.md D.101, PHYS.42): physics at
// ticks under half a millisecond, and a swimmer at a NaN place, logged with
// "DXSHORT:".
//
// On 01_NYC_UNATCOIsland, 3 s in, the player held where it starts, then by
// DXCAP_CONFIG Only:
//   a. TimeDilation 0.02 (each actor tick some 0.3 ms at 60 frames a
//      second, the original's held to its 5 ms floor); a crate spawned 100
//      over the player's start and 64 aside, PHYS_Falling at rest: its Z
//      and velocity each console tick for 60 ticks. Expected: the
//      original's falls; DXEngine's stays at its height, Velocity.Z 0
//      (UActor_PhysFalling.cpp:165/247: the move under the double 0.0001 on
//      every axis is none, the velocity remade from it 0).
//   b. TimeDilation 0.02, the forward axis held (StandConsole's 120000):
//      the player's place each tick for 60 ticks. Expected: the original's
//      walks; DXEngine's does not move (UActor_PhysWalking.cpp:109/300).
//   c. TimeDilation 1; the player's bCollideWorld off (so SetLocation fits
//      nothing), PHYS_Swimming, SetLocation to (N,N,N), N the square root of
//      -1 (NaNConsole's NaN): "DXSHORT: c set" then a line a tick. Expected
//      in DXEngine: no line after "set" -- the game hangs in
//      UPawn::FindWaterLine (UPawn_ReachDeusEx.cpp:216), its halving never
//      under a unit with NaN ends. The original: needs the run.
// Then TimeDilation 1 and an exit.
//=============================================================================
class ShortTickConsole extends Console;

var config string Only;
var float MapTime;
var int Phase, Ticks;
var string CurrentMap;
var Actor Crate;
var vector Start;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;
	local float N;
	local vector V;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	if (DeusExPlayer(P) != None)
	{
		if (DeusExPlayer(P).dataLinkPlay != None)
			DeusExPlayer(P).dataLinkPlay.AbortDataLink();
		if (DeusExPlayer(P).conPlay != None)
			DeusExPlayer(P).conPlay.TerminateConversation();
	}

	if (Phase == 0 && !(M ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Phase = 1;
		return;
	}
	if (Phase <= 1 && M ~= "01_NYC_UNATCOIsland" && MapTime > 3.0)
	{
		Start = P.Location;
		Ticks = 0;
		if (Only == "" || Only == "a")
		{
			P.Level.TimeDilation = 0.02;
			Crate = P.Spawn(class'CrateUnbreakableSmall',,, Start + vect(64,0,100));
			if (Crate != None)
			{
				Crate.Velocity = vect(0,0,0);
				Crate.SetPhysics(PHYS_Falling);
			}
			Log("DXSHORT: a crate " $ Crate $ " at " $ Start + vect(64,0,100));
			Phase = 2;
		}
		else if (Only == "b")
		{
			P.Level.TimeDilation = 0.02;
			Log("DXSHORT: b start " $ Start);
			Phase = 3;
		}
		else if (Only == "c")
		{
			N = Sqrt(-1.0);
			V.X = N;
			V.Y = N;
			V.Z = N;
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_Swimming);
			P.SetLocation(V);
			Log("DXSHORT: c set, location " $ P.Location $ " physics " $ int(P.Physics) $ " zone " $ P.Region.Zone);
			Phase = 4;
		}
		return;
	}
	if (Phase == 2)
	{
		if (Crate != None)
			Log("DXSHORT: a tick " $ Ticks $ " delta " $ Delta $ " z " $ Crate.Location.Z $ " velocity " $ Crate.Velocity $ " physics " $ int(Crate.Physics));
		if (++Ticks >= 60)
			Phase = 9;
	}
	else if (Phase == 3)
	{
		P.aBaseY = 120000.0;
		Log("DXSHORT: b tick " $ Ticks $ " delta " $ Delta $ " moved " $ VSize(P.Location - Start) $ " velocity " $ P.Velocity $ " physics " $ int(P.Physics));
		if (++Ticks >= 60)
			Phase = 9;
	}
	else if (Phase == 4)
	{
		Log("DXSHORT: c tick " $ Ticks $ " location " $ P.Location $ " physics " $ int(P.Physics));
		if (++Ticks >= 10)
			Phase = 9;
	}
	if (Phase == 9)
	{
		P.aBaseY = 0;
		P.Level.TimeDilation = 1.0;
		Log("DXSHORT: done");
		P.ConsoleCommand("exit");
		Phase = 10;
	}
}
