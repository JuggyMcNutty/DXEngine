//=============================================================================
// Vm29Console: the VM edges of bugs.md VM.29, logged with "DXVM29:". 2 s in,
// from wherever the run starts: a CapVm29Probe spawned and its checks run
// (RunChecks); then the probe sent to its state B, with the label Here,
// B's BeginState going to C and then to no state; then the Outer of the
// probe, of the level, of the player, of a texture in no group and of an
// object new(None) makes; then an exit.
//
// The original's CallFunction (Core.dll 0x1013f4e0) steps each argument
// into the callee's frame as it comes to it, a static array whole
// (execLocalVariable 0x1012ebc0, CopyCompleteValue), and copies the out
// ones back after, every element, for a call ProcessInternal (0x1013f690)
// holds off too. Its
// GotoState (0x1012e8f0) clears RF_StateChanged only before an EndState or
// BeginState whose probe is on, so the GotoState to no state, from C,
// which has no EndState, leaves the flag C's set: the outer GotoState is
// preempted and logs nothing. Its objects' outers end at their packages: a
// level's at its map's, new(None)'s at Transient (GetTransientPackage,
// 0x101573e0).
//=============================================================================
class Vm29Console extends Console;

var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	local CapVm29Probe P;
	local Object O;
	local Texture T;

	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	P = Viewport.Actor.Spawn(class'CapVm29Probe',,, Viewport.Actor.Location + vect(0, 0, 200));
	if (P == None)
	{
		Log("DXVM29: no room for the probe");
	}
	else
	{
		P.RunChecks();
		P.GotoState('B', 'Here');
		Log("DXVM29: after GotoState B Here: in " $ P.GetStateName());
		Log("DXVM29: the probe's Outer " $ P.Outer);
	}
	Log("DXVM29: the level's Outer " $ Viewport.Actor.Level.Outer);
	Log("DXVM29: the player's Outer " $ Viewport.Actor.Outer);
	T = Texture(DynamicLoadObject("CoreTexMetal.Chainlnk_01", class'Texture'));
	Log("DXVM29: " $ T $ "'s Outer " $ T.Outer);
	O = new(None) class'CapNewProbe';
	Log("DXVM29: new(None)'s Outer " $ O.Outer);
	Log("DXVM29: done, exiting");
	Viewport.Actor.ConsoleCommand("exit");
}
