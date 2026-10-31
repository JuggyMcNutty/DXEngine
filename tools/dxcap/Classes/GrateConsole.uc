//=============================================================================
// GrateConsole: the grates under Castle Clinton on 02_NYC_BatteryPark, five
// movers pitched on their sides that slide up 62 units, met by the player's
// box -- logged with "DXGRATE:" (docs/DEVELOPMENT.md, scripted runs).
//
// 3 s into the map: each grate's keys, rotation and pivot, and the actors
// whose event is its tag; then the player's box swept 16 units along X and
// along Y from each point of a grid 192 units round each grate's box, on the
// floor under it: every hit, with what it met. Then the conversation the map
// opens in ended, and the player walked in the room each grate closes a duct
// from: put 60 units before DeusExMover31's face and walked into it, then
// put 8 units clear of that face and walked along it, past the grate; the
// same at DeusExMover13, turned a quarter more, 14 units clear: its place
// every tenth of a second and where it comes to rest. Then an exit.
//=============================================================================
class GrateConsole extends Console;

var float MapTime, StepTime, LogTime;
var int Step, Walk;
var string CurrentMap;
var vector WalkFrom[4];
var int WalkYaw[4];
var string WalkName[4];

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

function bool IsGrate(Mover M)
{
	return M.Name == 'DeusExMover13' || M.Name == 'DeusExMover31' || M.Name == 'DeusExMover34'
	    || M.Name == 'DeusExMover35' || M.Name == 'DeusExMover36';
}

function Detail(PlayerPawn P, Mover M)
{
	local Actor A;
	local vector Min, Max;
	local int i;

	M.GetBoundingBox(Min, Max);
	Log("DXGRATE: grate " $ M.Name $ " box " $ Min $ " to " $ Max $ " keys " $ M.NumKeys $ " base " $ M.BasePos
	    $ " rotation " $ M.Rotation $ " prepivot " $ M.PrePivot $ " state " $ M.GetStateName());
	for (i = 0; i < M.NumKeys && i < 8; i++)
		Log("DXGRATE: grate " $ M.Name $ " key " $ i $ " " $ M.KeyPos[i] $ " " $ M.KeyRot[i]);
	foreach P.AllActors(class'Actor', A)
		if (A.Event == M.Tag && M.Tag != '')
			Log("DXGRATE: grate " $ M.Name $ " switched by " $ A.Name $ " (" $ A.Class.Name $ ")");
}

// The player's box swept 16 units along X and along Y from each point of a
// grid 192 units round a mover's box, on the floor under it: every hit.
function Sweep(PlayerPawn P, Mover M)
{
	local vector Min, Max, Start, HitLocation, HitNormal, Extent;
	local Actor Hit;
	local int X, Y;

	M.GetBoundingBox(Min, Max);
	Extent.X = P.CollisionRadius;
	Extent.Y = P.CollisionRadius;
	Extent.Z = P.CollisionHeight;
	Start.Z = Min.Z + P.CollisionHeight + 1;
	for (X = int(Min.X / 16) * 16 - 192; X <= Max.X + 192; X += 16)
	{
		for (Y = int(Min.Y / 16) * 16 - 192; Y <= Max.Y + 192; Y += 16)
		{
			Start.X = X;
			Start.Y = Y;
			Hit = P.Trace(HitLocation, HitNormal, Start + vect(16,0,0), Start, true, Extent);
			if (Hit != None)
				Log("DXGRATE: sweep " $ M.Name $ " " $ X $ " " $ Y $ " +X " $ Hit.Name $ " at " $ HitLocation $ " normal " $ HitNormal);
			Hit = P.Trace(HitLocation, HitNormal, Start + vect(0,16,0), Start, true, Extent);
			if (Hit != None)
				Log("DXGRATE: sweep " $ M.Name $ " " $ X $ " " $ Y $ " +Y " $ Hit.Name $ " at " $ HitLocation $ " normal " $ HitNormal);
		}
	}
}

// The walks, in the room each grate closes a duct from: DeusExMover31 is 8
// deep along X (-504 to -496) and 64 along Y (-2560 to -2496), its room past
// -496; DeusExMover13 8 deep along Y (-1520 to -1512) and 64 along X (-656
// to -592), its room past -1512.
function Walks(PlayerPawn P)
{
	WalkName[0] = "into DeusExMover31";
	WalkFrom[0] = vect(-436, -2528, -400);
	WalkYaw[0] = 32768;
	WalkName[1] = "along DeusExMover31";
	WalkFrom[1] = vect(-468, -2640, -400);
	WalkYaw[1] = 16384;
	WalkName[2] = "into DeusExMover13";
	WalkFrom[2] = vect(-624, -1452, -400);
	WalkYaw[2] = 49152;
	WalkName[3] = "along DeusExMover13";
	WalkFrom[3] = vect(-680, -1478, -400);
	WalkYaw[3] = 0;
}

// On the floor a line straight down from the grates' middle height finds
// at the spot.
function Put(PlayerPawn P, int i)
{
	local rotator R;
	local vector Spot, HitLocation, HitNormal;
	local Actor Hit;
	local bool bPut;

	Spot = WalkFrom[i];
	Hit = P.Trace(HitLocation, HitNormal, Spot - vect(0,0,200), Spot, false);
	if (Hit != None)
		Spot.Z = HitLocation.Z + P.CollisionHeight + 1;
	R.Yaw = WalkYaw[i];
	bPut = P.SetLocation(Spot);
	P.SetRotation(R);
	P.ViewRotation = R;
	P.Velocity = vect(0,0,0);
	P.SetPhysics(PHYS_Falling);
	Log("DXGRATE: walk " $ WalkName[i] $ " floor " $ Hit $ " at " $ HitLocation.Z $ ", put " $ bPut $ " at " $ P.Location);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Mover Mv;
	local string M;

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
	StepTime += Delta;
	if (!(M ~= "02_NYC_BatteryPark"))
	{
		if (MapTime > 3.0 && Step == 0)
		{
			P.ConsoleCommand("open 02_NYC_BatteryPark");
			Step = 1;
		}
		return;
	}
	P.ReducedDamageType = 'All';
	if (Step <= 1)
	{
		if (MapTime > 3.0)
		{
			foreach P.AllActors(class'Mover', Mv)
				if (IsGrate(Mv))
					Detail(P, Mv);
			foreach P.AllActors(class'Mover', Mv)
				if (IsGrate(Mv))
					Sweep(P, Mv);
			// The map opens in a conversation, which holds the player
			Log("DXGRATE: the player " $ P.GetStateName() $ ", the conversation ended");
			if (DeusExPlayer(P) != None)
				DeusExPlayer(P).AbortConversation(True);
			Walks(P);
			Put(P, 0);
			StepTime = 0;
			Step = 2;
		}
	}
	else if (Step == 2)
	{
		// Landed, half a second; then walked forward, as a held key walks
		// it, a second and a half (StandConsole)
		if (StepTime > 0.5)
		{
			P.aBaseY = 6000.0;
			LogTime += Delta;
			if (LogTime >= 0.1)
			{
				LogTime = 0;
				Log("DXGRATE: walk " $ WalkName[Walk] $ " at " $ (StepTime - 0.5) $ " " $ P.Location $ " " $ P.GetStateName() $ " physics " $ P.Physics);
			}
		}
		if (StepTime > 2.0)
		{
			P.aBaseY = 0.0;
			StepTime = 0;
			Step = 3;
		}
	}
	else if (Step == 3)
	{
		if (StepTime > 0.5)
		{
			Log("DXGRATE: walk " $ WalkName[Walk] $ " rests at " $ P.Location);
			Walk++;
			if (Walk < 4)
			{
				Put(P, Walk);
				StepTime = 0;
				LogTime = 0;
				Step = 2;
			}
			else
			{
				Log("DXGRATE: done, exiting");
				P.ConsoleCommand("exit");
				Step = 4;
			}
		}
	}
}
