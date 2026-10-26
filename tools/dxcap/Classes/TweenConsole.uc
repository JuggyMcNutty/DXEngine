//=============================================================================
// TweenConsole: the tween into an animation, logged with "DXTWEEN:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a market tarp (Blowing, 15 frames,
// and Still, one, in its mesh) spawned over the player, then in one tick:
// PlayAnim of Blowing with nothing playing; LoopAnim of Blowing at 2 with
// the default tween; the same loop again at 1; TweenAnim of Still over
// 0.3 s, then LoopAnim of Blowing with the default; PlayAnim of Blowing
// with no tween (0), then LoopAnim with none; PlayAnim at -1 (a
// velocity-scaled rate) with no tween, then LoopAnim with the default;
// LoopAnim with a tween of 0.1 s and a MinRate of 0.5; PlayAnim of Still,
// one frame, with the default and over 0.5 s; and calls of a sequence the
// mesh lacks. Its frame, rate, tween rate, old rate, least rate, last frame
// and flags logged after each; then an exit. What is compared between the
// engines: every line, exactly.
//=============================================================================
class TweenConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXTWEEN: " $ S);
}

function Report(string When, Actor A)
{
	Say(When $ ": sequence " $ A.AnimSequence $ " frame " $ A.AnimFrame $ " rate " $ A.AnimRate $ " tween " $ A.TweenRate $ " old " $ A.OldAnimRate $ " min " $ A.AnimMinRate $ " last " $ A.AnimLast $ " loop " $ A.bAnimLoop $ " notify " $ A.bAnimNotify $ " finished " $ A.bAnimFinished);
}

function Probes()
{
	local HKMarketTarp T;

	T = Viewport.Actor.Spawn(class'HKMarketTarp',,, Viewport.Actor.Location + vect(0, 0, 300));
	if (T == None)
	{
		Say("no tarp spawned");
		return;
	}
	Report("spawned", T);
	T.PlayAnim('Blowing');
	Report("PlayAnim Blowing from nothing", T);
	T.LoopAnim('Blowing', 2.0);
	Report("LoopAnim Blowing at 2, default tween", T);
	T.LoopAnim('Blowing', 1.0);
	Report("the same loop at 1", T);
	T.TweenAnim('Still', 0.3);
	Report("TweenAnim Still over 0.3", T);
	T.LoopAnim('Blowing');
	Report("LoopAnim Blowing after a tween, default", T);
	T.PlayAnim('Blowing', 1.0, 0.0);
	Report("PlayAnim Blowing, tween 0", T);
	T.LoopAnim('Blowing', 1.0, 0.0);
	Report("LoopAnim Blowing, tween 0", T);
	T.PlayAnim('Blowing', -1.0, 0.0);
	Report("PlayAnim Blowing at -1, tween 0", T);
	T.LoopAnim('Blowing');
	Report("LoopAnim Blowing after a velocity rate, default", T);
	T.LoopAnim('Blowing', 1.0, 0.1, 0.5);
	Report("LoopAnim Blowing, tween 0.1, MinRate 0.5", T);
	T.PlayAnim('Still');
	Report("PlayAnim Still, one frame, default", T);
	T.PlayAnim('Still', 1.0, 0.5);
	Report("PlayAnim Still over 0.5", T);
	T.LoopAnim('NoSuch');
	T.PlayAnim('NoSuch');
	T.TweenAnim('NoSuch', 0.1);
	Report("after calls of NoSuch", T);
	T.Destroy();
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes();
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
