//=============================================================================
// CapRoleActor: RoleGatesConsole's actors (bugs.md NET.21) -- an Info of the
// package's own, touching nothing, that Setup makes one of the console's
// kinds, its roles set last: moving 40 along X (DumbFall falling), with a
// life span of 2.2 s, a timer every 0.5 s and state code that passes every
// 0.5 s in a state that is not simulated (SimState's in one that is); Lapse
// has only state code, a pass a second. It counts its Ticks, Timers and
// state code passes, and logs with "DXROLES:" each pass and its expiry, at
// the time from its setup.
//=============================================================================
class CapRoleActor extends Info;

var string Who;
var int Ticks, Timers, Passes;
var float Started, Wait;
var vector StartedAt;

function Note(string S)
{
	Log("DXROLES: " $ Who $ " " $ S $ " at " $ (Level.TimeSeconds - Started));
}

// What it did: whether it ticked, its timers and passes, how far it moved
// along X and whether it fell
function string Did()
{
	return "ticked " $ (Ticks > 0) $ ", " $ Timers $ " Timers, " $ Passes $ " state code passes, moved " $ (Location.X - StartedAt.X) $ " along X, fell " $ (StartedAt.Z - Location.Z > 1.0);
}

function Setup(string Kind)
{
	Who = Kind;
	Started = Level.TimeSeconds;
	StartedAt = Location;
	Wait = 0.5;
	if (Kind == "Lapse")
		Wait = 1.0;
	else
	{
		if (Kind == "DumbFall")
			SetPhysics(PHYS_Falling);
		else
		{
			SetPhysics(PHYS_Projectile);
			Velocity = vect(40, 0, 0);
		}
		LifeSpan = 2.2;
		SetTimer(0.5, true);
	}
	if (Kind == "SimState")
		GotoState('SimProbing');

	if (Kind == "Sim" || Kind == "SimState")
	{
		RemoteRole = ROLE_Authority;
		Role = ROLE_SimulatedProxy;
	}
	else if (Kind == "Auto")
	{
		RemoteRole = ROLE_Authority;
		Role = ROLE_AutonomousProxy;
	}
	else if (Kind == "Dumb" || Kind == "DumbFall")
	{
		RemoteRole = ROLE_Authority;
		Role = ROLE_DumbProxy;
	}
	else if (Kind == "Served")
	{
		RemoteRole = ROLE_AutonomousProxy;
		Role = ROLE_Authority;
	}
	else if (Kind == "Odd")
	{
		RemoteRole = ROLE_AutonomousProxy;
		Role = ROLE_SimulatedProxy;
	}
	else
	{
		// Auth and Lapse: a standalone actor's
		RemoteRole = ROLE_SimulatedProxy;
		Role = ROLE_Authority;
	}
}

// Lapse's turns: a client's copy, then the authority again
function MakeProxy()
{
	RemoteRole = ROLE_Authority;
	Role = ROLE_SimulatedProxy;
	Note("made a simulated proxy");
}

function MakeAuthority()
{
	RemoteRole = ROLE_SimulatedProxy;
	Role = ROLE_Authority;
	Note("the authority again");
}

function Tick(float Delta)
{
	Ticks++;
}

function Timer()
{
	Timers++;
}

event Expired()
{
	Note("expired: " $ Did());
}

auto state Probing
{
Begin:
	Sleep(Wait);
	Passes++;
	Note("state code " $ Passes);
	Goto('Begin');
}

simulated state SimProbing
{
Begin:
	Sleep(Wait);
	Passes++;
	Note("simulated state code " $ Passes);
	Goto('Begin');
}

defaultproperties
{
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
