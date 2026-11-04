//=============================================================================
// CapAnimPawn: the AnimEnd net test's pawn (AnimEndServeConsole,
// AnimEndJoinConsole; bugs.md NET.15) -- a pawn of the package's own in the
// Hong Kong market tarp's mesh (Blowing, 15 frames at 15 a second, its
// AnimLast 0.9333; Still, one frame), always relevant, touching nothing, a
// simulated proxy on the client. The server's console plays its animations
// and sets its Stage, which goes to the client: 1 three one-shots, 2 a
// loop, 3 three tweens, 4 done. On each side it logs with "DXANIMEND:" each
// AnimEnd it is sent -- the original's AActor::Tick sends a simulated proxy
// pawn none (Engine.dll 0x103a1bdf) --, each animation as it starts (on the
// client as its SimAnim is unpacked), each stop and each loop's wrap; and at
// each change of Stage what the stage before it saw. Its functions are all
// simulated, so a client's copy runs them: one that is not is held off
// there in either engine.
//=============================================================================
class CapAnimPawn extends Pawn;

// The server's stage: 0 placed, 1 one-shots, 2 a loop, 3 tweens, 4 done
var byte Stage;

// This side's: the stage last seen, its counts and the whole run's, and
// the animation as the last tick left it
var byte SeenStage;
var int Ends, Wraps, Stops;
var int AllEnds, AllWraps, AllStops;
var bool bSeen, bDone, bWasPlaying;
var plane LastSim;
var name LastSequence;
var float LastFrame;

replication
{
	reliable if (Role == ROLE_Authority)
		Stage;
}

// A line of this side's: the server's (the authority) or the client's
simulated function Mark(string S)
{
	local string Side;

	if (Role == ROLE_Authority)
		Side = "server";
	else
		Side = "client";
	Log("DXANIMEND: " $ Side $ " " $ S $ " at " $ Level.TimeSeconds);
}

// Moving as the engine's tick has it: while a sequence is set, a tween at
// a rate from a negative frame, else a rate
simulated function bool Playing()
{
	if (AnimSequence == '')
		return false;
	if (AnimFrame < 0.0)
		return TweenRate != 0.0;
	return AnimRate != 0.0;
}

simulated function AnimEnd()
{
	Ends++;
	Mark("animend " $ (AllEnds + Ends) $ " " $ AnimSequence $ " frame " $ AnimFrame $ " rate " $ AnimRate $ " finished " $ bAnimFinished $ " loop " $ bAnimLoop);
}

simulated function Tick(float Delta)
{
	local bool bNew;

	if (!bSeen)
	{
		bSeen = true;
		Mark("has the pawn: role " $ Role $ ", remote role " $ RemoteRole $ ", bIsPawn " $ bIsPawn $ ", net mode " $ Level.NetMode $ ", stage " $ Stage);
		SeenStage = Stage;
		LastSim = SimAnim;
		LastSequence = AnimSequence;
		LastFrame = AnimFrame;
		bWasPlaying = Playing();
		return;
	}

	// What the stage before saw, once the server's next one reaches this side
	if (Stage != SeenStage)
	{
		Mark("stage " $ SeenStage $ " saw " $ Ends $ " AnimEnds, " $ Wraps $ " wraps, " $ Stops $ " stops");
		AllEnds += Ends;
		AllWraps += Wraps;
		AllStops += Stops;
		Ends = 0;
		Wraps = 0;
		Stops = 0;
		SeenStage = Stage;
		if (Stage == 4 && !bDone)
		{
			bDone = true;
			Mark("done: " $ AllEnds $ " AnimEnds, " $ AllWraps $ " wraps, " $ AllStops $ " stops");
		}
	}

	// A new animation -- the server's call, or on the client its SimAnim
	// unpacked --, else a loop's wrap or a stop
	bNew = SimAnim.X != LastSim.X || SimAnim.Y != LastSim.Y || SimAnim.Z != LastSim.Z || SimAnim.W != LastSim.W || AnimSequence != LastSequence;
	if (bNew)
		Mark("start " $ AnimSequence $ " frame " $ AnimFrame $ " rate " $ AnimRate $ " tween " $ TweenRate $ " last " $ AnimLast $ " loop " $ bAnimLoop);
	else
	{
		if (bAnimLoop && AnimFrame < LastFrame)
		{
			Wraps++;
			Mark("wrap " $ (AllWraps + Wraps) $ " frame " $ AnimFrame);
		}
		if (bWasPlaying && !Playing())
		{
			Stops++;
			Mark("stop " $ (AllStops + Stops) $ " " $ AnimSequence $ " frame " $ AnimFrame $ " rate " $ AnimRate $ " finished " $ bAnimFinished);
		}
	}
	LastSim = SimAnim;
	LastSequence = AnimSequence;
	LastFrame = AnimFrame;
	bWasPlaying = Playing();
}

defaultproperties
{
	DrawType=DT_Mesh
	Mesh=LodMesh'DeusExDeco.HKMarketTarp'
	AnimSequence=Still
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
