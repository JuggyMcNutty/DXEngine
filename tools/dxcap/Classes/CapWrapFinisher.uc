//=============================================================================
// CapWrapFinisher: FinishWrapConsole's actor (bugs.md AI.15), a market tarp
// whose state code waits in FinishAnim while its own calls change the
// animation. Stage 1: Blowing played at 0.25, its Timer looping Blowing at
// 1 0.3 s into the wait, the loop's pass over AnimLast some 0.93 s later
// the wait's end (Engine.dll 0x103a1e20); then a second FinishAnim, which
// unloops it at its last frame and does not wait. Stage 2: Blowing played
// at 2, its AnimEnd playing Blowing at 1 once, which the wait waits for too
// (execPollFinishAnim 0x103e00a0). Each AnimEnd, each call and each
// FinishAnim's return logged with "DXWRAP:", the time from the stage's
// start, the frame, the rate, bAnimFinished and bAnimLoop. Its Tick is its
// own, so the tarp's random blowing never starts.
//=============================================================================
class CapWrapFinisher extends HKMarketTarp;

var int Stage;
var float StageStart;
var bool bReplay;

function Say(string S)
{
	Log("DXWRAP: stage " $ Stage $ " " $ S $ " at " $ (Level.TimeSeconds - StageStart) $ " frame " $ AnimFrame $ " rate " $ AnimRate $ " finished " $ bAnimFinished $ " loop " $ bAnimLoop);
}

function Tick(float DeltaTime)
{
}

function Timer()
{
	LoopAnim('Blowing', 1.0, 0.0);
	Say("Timer loops Blowing at 1");
}

function AnimEnd()
{
	Say("AnimEnd");
	if (bReplay)
	{
		bReplay = false;
		PlayAnim('Blowing', 1.0, 0.0);
		Say("AnimEnd plays Blowing at 1");
	}
}

auto state Probing
{
Begin:
	Sleep(0.5);
	Stage = 1;
	StageStart = Level.TimeSeconds;
	PlayAnim('Blowing', 0.25, 0.0);
	SetTimer(0.3, false);
	Say("PlayAnim Blowing at 0.25, the Timer in 0.3 s");
	FinishAnim();
	Say("after FinishAnim");
	FinishAnim();
	Say("after a second FinishAnim");
	Sleep(0.5);
	Stage = 2;
	StageStart = Level.TimeSeconds;
	bReplay = true;
	PlayAnim('Blowing', 2.0, 0.0);
	Say("PlayAnim Blowing at 2, its AnimEnd to play it again");
	FinishAnim();
	Say("after FinishAnim");
	Sleep(2.0);
	Stage = 3;
	Say("done");
}

defaultproperties
{
	bRandomize=True
	bStasis=False
	Physics=PHYS_None
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
}
