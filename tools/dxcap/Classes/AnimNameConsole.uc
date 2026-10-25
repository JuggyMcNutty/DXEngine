//=============================================================================
// AnimNameConsole: an animation name the mesh lacks, logged with
// "DXANIMNAME:" (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a market tarp (Blowing and Still in
// its mesh) spawned over the player and asked HasAnim and GetAnimGroup for
// Blowing and for NoSuch; then PlayAnim, LoopAnim, TweenAnim and
// PlayBlendAnim of NoSuch after a LoopAnim of Blowing, its AnimSequence,
// IsAnimating, AnimRate and AnimFrame logged after each; and an exit. What
// is compared between the engines: every line, exactly, and the log's
// "not found" lines.
//=============================================================================
class AnimNameConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXANIMNAME: " $ S);
}

function Report(string When, Actor A)
{
	Say(When $ ": sequence " $ A.AnimSequence $ " animating " $ A.IsAnimating() $ " rate " $ A.AnimRate $ " frame " $ A.AnimFrame $ " blend 0 " $ A.BlendAnimSequence[0]);
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
	Say("has Blowing " $ T.HasAnim('Blowing') $ " NoSuch " $ T.HasAnim('NoSuch') $ "; group of Blowing " $ T.GetAnimGroup('Blowing') $ " of NoSuch " $ T.GetAnimGroup('NoSuch'));
	T.LoopAnim('Blowing', 1.0);
	Report("looping Blowing", T);
	T.PlayAnim('NoSuch');
	Report("after PlayAnim NoSuch", T);
	T.LoopAnim('Blowing', 1.0);
	T.LoopAnim('NoSuch');
	Report("after LoopAnim NoSuch", T);
	T.LoopAnim('Blowing', 1.0);
	T.TweenAnim('NoSuch', 0.2);
	Report("after TweenAnim NoSuch", T);
	T.LoopAnim('Blowing', 1.0);
	T.PlayBlendAnim('NoSuch', 1.0, 0.0, 0);
	Report("after PlayBlendAnim NoSuch", T);
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
