//=============================================================================
// CapUnpackPawn: the unpacking net test's pawn (UnpackServeConsole,
// UnpackJoinConsole; bugs.md NET.27) -- the Hong Kong market tarp's mesh on
// a pawn of the package's own with no animation, so that nothing moves its
// frame on either side, always relevant, touching nothing, a simulated
// proxy on the client. The server's console sets its SimAnim and its Stage,
// which go to the client; there, as each SimAnim arrives, the engine
// unpacks it into the frame, rate, tween rate and last frame, which it logs
// with "DXUNPACK:", each exactly: its significand, a whole number from 2^23
// to 2^24, times a power of 2.
//=============================================================================
class CapUnpackPawn extends Pawn;

// The server's stage: 0 placed, 1 to 3 a SimAnim each, 4 done
var byte Stage;

var byte SeenStage;
var bool bSeen, bDone;
var plane LastSim;

replication
{
	reliable if (Role == ROLE_Authority)
		Stage;
}

// A float exactly, as m*2^e: halved or doubled into 2^23 to 2^24, each step
// exact at any precision
simulated function string Exact(float F)
{
	local int E;
	local string S;

	if (F == 0.0)
		return "0";
	if (F < 0.0)
	{
		S = "-";
		F = -F;
	}
	while (F >= 16777216.0)
	{
		F *= 0.5;
		E++;
	}
	while (F < 8388608.0)
	{
		F *= 2.0;
		E--;
	}
	return S $ int(F) $ "*2^" $ E;
}

// A line of this side's: the server's (the authority) or the client's
simulated function Mark(string S)
{
	local string Side;

	if (Role == ROLE_Authority)
		Side = "server";
	else
		Side = "client";
	Log("DXUNPACK: " $ Side $ " " $ S $ " at " $ Level.TimeSeconds);
}

simulated function Tick(float Delta)
{
	if (!bSeen)
	{
		bSeen = true;
		Mark("has the pawn: role " $ Role $ ", remote role " $ RemoteRole $ ", sequence " $ AnimSequence $ ", stage " $ Stage);
		SeenStage = Stage;
		LastSim = SimAnim;
		return;
	}

	// On the client each SimAnim as the engine unpacked it
	if (Role < ROLE_Authority && (SimAnim.X != LastSim.X || SimAnim.Y != LastSim.Y || SimAnim.Z != LastSim.Z || SimAnim.W != LastSim.W))
		Mark("SimAnim " $ SimAnim.X $ "," $ SimAnim.Y $ "," $ SimAnim.Z $ "," $ SimAnim.W $ ": frame " $ Exact(AnimFrame)
			$ ", rate " $ Exact(AnimRate) $ ", tween " $ Exact(TweenRate) $ ", last " $ Exact(AnimLast) $ ", loop " $ bAnimLoop);
	LastSim = SimAnim;

	if (Stage != SeenStage)
	{
		SeenStage = Stage;
		Mark("stage " $ Stage);
		if (Stage == 4)
			bDone = true;
	}
}

defaultproperties
{
	DrawType=DT_Mesh
	Mesh=LodMesh'DeusExDeco.HKMarketTarp'
	RemoteRole=ROLE_SimulatedProxy
	bAlwaysRelevant=True
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	bProjTarget=False
	Physics=PHYS_None
	CollisionRadius=72.000000
	CollisionHeight=15.000000
}
