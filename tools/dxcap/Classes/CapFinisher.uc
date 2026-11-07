//=============================================================================
// CapFinisher: FinishConsole's actor, a market tarp whose state code calls
// FinishAnim after a PlayAnim, after a TweenAnim with a time, with nothing
// animating, and after a LoopAnim, logging the level's time as each
// FinishAnim returns, with "DXFINISH:". Its Tick is its own, so the tarp's
// random blowing never starts during a wait.
//=============================================================================
class CapFinisher extends HKMarketTarp;

function Tick(float DeltaTime)
{
}

function Say(string S)
{
	Log("DXFINISH: " $ S $ " at " $ Level.TimeSeconds $ " frame " $ AnimFrame $ " rate " $ AnimRate $ " finished " $ bAnimFinished);
}

auto state Probing
{
Begin:
	Sleep(0.5);
	Say("start");
	PlayAnim('Blowing', 2.0);
	Say("PlayAnim Blowing at 2");
	FinishAnim();
	Say("after FinishAnim");
	TweenAnim('Still', 0.5);
	Say("TweenAnim Still over 0.5 s");
	FinishAnim();
	Say("after FinishAnim");
	Say("nothing animating");
	FinishAnim();
	Say("after FinishAnim");
	LoopAnim('Blowing', 2.0);
	Say("LoopAnim Blowing at 2");
	FinishAnim();
	Say("after FinishAnim");
	FinishAnim();
	Say("after a second FinishAnim");
	Say("done");
}

defaultproperties
{
	bRandomize=True
}
