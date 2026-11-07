//=============================================================================
// LineEdgeConsole: the line checks' edges and precision (bugs.md PHYS.22),
// logged with "DXLINE:". On TargetMap ([DXCapture.LineEdgeConsole],
// 01_NYC_UNATCOIsland by default), datalinks and conversations silenced,
// 2 s in: the player taken out of the world where it stands, and a
// CapLineProbe set up there in the console's tick, which then asks its
// checks and logs each answer twice, in the console's tick and in its own
// (the level's): the original's x87 works their sums out at 24 bits in the
// first and at 64 in the second.
//   1. VisibleActors of the probe's three marks, 50, 100 and 150 units from
//      the player's place along the first yaw with 200 units free, within
//      120, -120, 100, 0 and a NaN (the square root of -1): each found mark's
//      distance. The original's test is the squared distance under the
//      radius squared (Engine.dll 0x103e53d0), so -120 reaches as far as 120,
//      and 0 and a NaN have no limit (its C3).
//   2. Inside solid, 8 units behind the face of the floor under the player:
//      FastTrace from there to itself, to 0.005 and to 0.02 units along X,
//      and VisibleActors from there with no limit, a mark set there too. The
//      original walks every line, one of no length too, and from inside
//      solid each is blocked: none clear, no mark found.
//   3. The first wall the eye meets within 2000 units, by yaws 2048 apart
//      from its facing, that lies along neither X nor Y: FastTrace of the
//      lines from the eye to 81 points across the wall's face, from 0.002
//      units in front of it to 0.002 behind, and of the same lines walked
//      from the points; each answer a 1 for clear, in one line a way. Where
//      the answers turn over is where the original's floats put the plane.
//   4. EAdjustJump (SuggestJumpVelocity's jump) of a CapReacher, out of the
//      world at the player's place, toward six destinations at six upward
//      speeds: the velocity it answers.
// The spots, the points and the destinations are worked out once, in the
// console's tick, so both ticks ask the same. Then an exit.
//=============================================================================
class LineEdgeConsole extends Console;

var config string TargetMap;

var float MapTime, StepTime;
var int Step;
var string CurrentMap;
var CapLineProbe Probe;

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

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;

	Super.Tick(Delta);
	if (Step == 3 || Viewport == None || Viewport.Actor == None)
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
	if (DXP != None)
	{
		if (DXP.dataLinkPlay != None)
			DXP.dataLinkPlay.AbortDataLink();
		if (DXP.conPlay != None)
			DXP.conPlay.TerminateConversation();
	}

	if (Step == 0)
	{
		// The original starts in its menu map.
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXLINE: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Step = -1;
			}
			return;
		}
		if (MapTime < 2.0)
			return;
		P.ReducedDamageType = 'All';
		P.SetCollision(false, false, false);
		P.bCollideWorld = false;
		P.SetPhysics(PHYS_None);
		Probe = P.Spawn(class'CapLineProbe');
		if (Probe == None)
		{
			Log("DXLINE: the probe did not spawn, exiting");
			P.ConsoleCommand("exit");
			Step = 3;
			return;
		}
		Probe.Setup(P);
		Probe.Run("console's tick");
		Step = 1;
		StepTime = 0;
	}
	else if (Step == -1)
	{
		if (CurrentMap ~= Target())
			Step = 0;
	}
	else if (Step == 1 && StepTime > 1.0)
	{
		Log("DXLINE: done, exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
