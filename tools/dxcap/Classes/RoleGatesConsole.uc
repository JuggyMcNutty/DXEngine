//=============================================================================
// RoleGatesConsole: actors given the roles a net game gives them, in single
// player (bugs.md NET.21, NET.22, NET.26; docs/DEVELOPMENT.md, scripted
// runs), logged with "DXROLES:". The original's AActor::Tick goes by the
// roles alone, whatever the net mode (Engine.dll 0x103a1aa0): an actor whose
// remote role is autonomous runs its state code and timer, nothing more, and
// with any role but the authority nothing (0x103a23ce); a dumb proxy only
// falls (0x103a2524); an autonomous proxy has no physics (0x103a2763); a
// proxy runs its state code, and counts down its latent action, in a
// simulated state alone (ProcessState, 0x103e5b9c). Its DestroyActor
// refuses an actor without the authority (0x10395c32-0x10395c44): a proxy
// whose life span runs out stays, and expires again each tick, its tick
// ending there before its physics; one that falls out of the world stays,
// its physics none; its script Destroy answers False.
//
// From wherever the run starts, 2 s in, nine CapRoleActors over the player,
// each moving 40 along X with a life span of 2.2 s, a timer every 0.5 s and
// state code passing every 0.5 s (CapRoleActor), as one of:
//   Auth      the authority, remote role simulated (a standalone actor's)
//   Sim       a simulated proxy, remote role the authority (a client's copy)
//   SimState  the same, its state code in a simulated state
//   Auto      an autonomous proxy, remote role the authority
//   Dumb      a dumb proxy, remote role the authority
//   DumbFall  the same, falling
//   Served    the authority, remote role autonomous (a server's copy of a
//             client's pawn)
//   Odd       a simulated proxy, remote role autonomous
//   Lapse     the authority with only state code, a pass a second; made a
//             simulated proxy 0.5 s in, the authority again at 2.5 s
// and two CapRoleViewers, player pawns with the authority viewing a
// CapRoleTarget pawn (its view rotation 1234,23456,0, eye height 37.5, a
// CapRoleWeapon held with PlayerViewOffset 11,22,33): Viewer with the remote
// role autonomous, Control with the remote role simulated. At 1.5 s the
// target's view rotation becomes 2000,30000,0, its eye height 20 and its
// weapon None. The viewers' targets logged at 1 s and 3 s; at 3.25 s what
// each actor did, the ones still there destroyed, with what each Destroy
// answers, and an exit.
//
// The original's, so DXEngine's once fixed:
//   Auth, SimState: ticked, 4 Timers, 4 passes, expired at 2.2 after 88
//     along X; Sim the same with no pass; Auto too, not moving; Auth gone,
//     Sim, SimState and Auto expiring again each tick and there at 3.25,
//     6 Timers, SimState 6 passes, none moved since, Destroy False;
//   Dumb and Odd: nothing at all, not expired, Destroy False; DumbFall:
//     fell out of the world, nothing more, there at 3.25, Destroy False;
//   Served: not ticked, 6 Timers, 6 passes, not moving, not expired;
//   Lapse: one pass, at 3.0 (its sleep's second half after 2.5); both
//     Destroy True;
//   Viewer: not ticked, its targets the target's -- at 3 s 2000,30000,0, 20
//     and the weapon's 11,22,33 kept; Control: ticked, its targets 0.
//=============================================================================
class RoleGatesConsole extends Console;

var float RunTime, SetTime;
var int Phase;
var CapRoleActor A[9];
var string Kinds[9];
var CapRoleViewer Viewer, Control;
var CapRoleTarget Target;
var CapRoleWeapon Gun;

function Make(int i, string Kind)
{
	local PlayerPawn P;

	P = Viewport.Actor;
	Kinds[i] = Kind;
	A[i] = P.Spawn(class'CapRoleActor',,, P.Location + vect(0, 0, 300) + float(i) * vect(0, 40, 0));
	if (A[i] == None)
	{
		Log("DXROLES: " $ Kind $ " not spawned");
		return;
	}
	A[i].Setup(Kind);
	Log("DXROLES: " $ Kind $ ": role " $ A[i].Role $ ", remote role " $ A[i].RemoteRole $ ", physics " $ A[i].Physics $ ", state " $ A[i].GetStateName() $ ", net mode " $ A[i].Level.NetMode);
}

function CapRoleViewer MakeViewer(string Who, vector At)
{
	local CapRoleViewer V;

	V = Viewport.Actor.Spawn(class'CapRoleViewer',,, At);
	if (V == None)
	{
		Log("DXROLES: " $ Who $ " not spawned");
		return None;
	}
	V.ViewTarget = Target;
	return V;
}

function LogTargets()
{
	if (Viewer != None)
		Log("DXROLES: Viewer's targets: " $ Viewer.Targets());
	if (Control != None)
		Log("DXROLES: Control's targets: " $ Control.Targets());
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float T;
	local int i;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	T = RunTime - SetTime;

	if (Phase == 0 && RunTime > 2.0)
	{
		Make(0, "Auth");
		Make(1, "Sim");
		Make(2, "SimState");
		Make(3, "Auto");
		Make(4, "Dumb");
		Make(5, "DumbFall");
		Make(6, "Served");
		Make(7, "Odd");
		Make(8, "Lapse");

		Target = P.Spawn(class'CapRoleTarget',,, P.Location + vect(0, -80, 300));
		Gun = P.Spawn(class'CapRoleWeapon',,, P.Location + vect(0, -80, 300));
		if (Target == None || Gun == None)
		{
			Log("DXROLES: no target or weapon spawned");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Gun.SetCollision(false, false, false);
		Gun.PlayerViewOffset = vect(11, 22, 33);
		Target.ViewRotation = rot(1234, 23456, 0);
		Target.EyeHeight = 37.5;
		Target.Weapon = Gun;

		Viewer = MakeViewer("Viewer", P.Location + vect(0, -120, 300));
		Control = MakeViewer("Control", P.Location + vect(0, -160, 300));
		if (Viewer != None)
			Viewer.RemoteRole = ROLE_AutonomousProxy;
		if (Control != None)
			Control.RemoteRole = ROLE_SimulatedProxy;
		if (Viewer != None && Control != None)
			Log("DXROLES: Viewer: role " $ Viewer.Role $ ", remote role " $ Viewer.RemoteRole $ "; Control: role " $ Control.Role $ ", remote role " $ Control.RemoteRole $ "; viewing " $ Target.Name);
		SetTime = RunTime;
		Phase = 1;
	}
	else if (Phase == 1 && T >= 0.5)
	{
		if (A[8] != None)
			A[8].MakeProxy();
		Phase = 2;
	}
	else if (Phase == 2 && T >= 1.0)
	{
		LogTargets();
		Phase = 3;
	}
	else if (Phase == 3 && T >= 1.5)
	{
		Target.ViewRotation = rot(2000, 30000, 0);
		Target.EyeHeight = 20.0;
		Target.Weapon = None;
		Log("DXROLES: the target's view rotation 2000,30000,0, eye height 20, no weapon");
		Phase = 4;
	}
	else if (Phase == 4 && T >= 2.5)
	{
		if (A[8] != None)
			A[8].MakeAuthority();
		Phase = 5;
	}
	else if (Phase == 5 && T >= 3.0)
	{
		LogTargets();
		Phase = 6;
	}
	else if (Phase == 6 && T >= 3.25)
	{
		for (i = 0; i < 9; i++)
		{
			if (A[i] == None)
				continue;
			// An expired actor's strings are gone with it in the original
			// (its properties' ExitProperties): its kind is the console's
			if (A[i].bDeleteMe)
				Log("DXROLES: " $ Kinds[i] $ " gone");
			else
			{
				Log("DXROLES: " $ Kinds[i] $ " at 3.25: " $ A[i].Did() $ ", role " $ A[i].Role);
				Log("DXROLES: " $ Kinds[i] $ " Destroy " $ A[i].Destroy());
			}
		}
		if (Viewer != None)
		{
			Log("DXROLES: Viewer ticked " $ (Viewer.Ticks > 0));
			Viewer.Destroy();
		}
		if (Control != None)
		{
			Log("DXROLES: Control ticked " $ (Control.Ticks > 0));
			Control.Destroy();
		}
		Target.Destroy();
		Gun.Destroy();
		Log("DXROLES: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
