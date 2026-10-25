//=============================================================================
// PassesConsole: how far an animation runs in one long tick, logged with
// "DXPASS:" (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a CapAnimated (a market tarp, its
// Blowing sequence looped at 20 a second) spawned over the player; a second
// later one frame held 2 s by the console's line traces (FrameConsole's),
// the actor's ticks getting at most 0.4 s of it; its ticks and AnimEnds
// logged throughout, with the level's time; three of the actor's ticks
// after the held frame, an exit. What is compared between the engines: the AnimEnds in the long
// tick -- each pass of the animation's tick ends at a notify or the
// sequence's end, and a tick gets four passes -- and the AnimFrame after it.
//=============================================================================
class PassesConsole extends Console;

var float RunTime;
var int Phase, TicksAtHold;
var CapAnimated Tarp;

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector HitLocation, HitNormal;
	local float Stalled;
	local int c, i, Spans;

	Super.Tick(Delta);
	if (Phase == 4 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Tarp = P.Spawn(class'CapAnimated',,, P.Location + vect(0, 0, 300));
		if (Tarp == None)
		{
			Log("DXPASS: no tarp spawned");
			Phase = 4;
			P.ConsoleCommand("exit");
			return;
		}
		Tarp.LoopAnim('Blowing', 20.0);
		Log("DXPASS: tarp spawned, Blowing looped at 20: rate " $ Tarp.AnimRate $ " last " $ Tarp.AnimLast);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Log("DXPASS: holding a frame at " $ P.Level.TimeSeconds);
		while (Stalled < 2.0 && Spans < 4000)
		{
			c = 0;
			clock(c);
			for (i = 0; i < 1000; i++)
			{
				P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
				P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
				P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
				P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
			}
			unclock(c);
			Stalled += CyclesToSeconds(c);
			Spans++;
		}
		Log("DXPASS: a frame held over 2 s: " $ (Stalled >= 2.0));
		TicksAtHold = Tarp.Ticks;
		Phase = 2;
	}
	else if (Phase == 2 && Tarp.Ticks >= TicksAtHold + 3)
	{
		Log("DXPASS: done, exiting");
		Tarp.Destroy();
		P.ConsoleCommand("exit");
		Phase = 4;
	}
}
