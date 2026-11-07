//=============================================================================
// CapSimPawn: SimRoleConsole's pawn (bugs.md NET.16) -- the market tarp's
// mesh on a pawn of the package's own, touching nothing, made a simulated
// proxy in single player by Simulate: its velocity, a life span, a looping
// timer and state code of its own. The original's AActor::Tick moves such a
// pawn along its velocity and runs its Tick, whatever the net mode
// (Engine.dll 0x103a1fd2), so its timer, state code, life span and physics
// never run. It logs with "DXSIMROLE:" each Timer, each pass of its state
// code and its expiry, and counts its Ticks.
//=============================================================================
class CapSimPawn extends Pawn;

var int Ticks, Timers, Passes;
var float Started;
var vector StartedAt;

function Note(string S)
{
	Log("DXSIMROLE: " $ S $ " at " $ (Level.TimeSeconds - Started));
}

function Simulate(vector V, float Life)
{
	Velocity = V;
	LifeSpan = Life;
	SetTimer(0.5, true);
	Started = Level.TimeSeconds;
	StartedAt = Location;
	Role = ROLE_SimulatedProxy;
}

function Tick(float Delta)
{
	Ticks++;
}

function Timer()
{
	Timers++;
	Note("Timer " $ Timers);
}

event Expired()
{
	Note("expired");
}

auto state Probing
{
Begin:
	Sleep(0.5);
	Passes++;
	Note("state code " $ Passes);
	Goto('Begin');
}

defaultproperties
{
	DrawType=DT_Mesh
	Mesh=LodMesh'DeusExDeco.HKMarketTarp'
	AnimSequence=Still
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
