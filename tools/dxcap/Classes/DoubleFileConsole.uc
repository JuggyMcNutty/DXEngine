//=============================================================================
// DoubleFileConsole (bugs.md PHYS.43): an actor
// filed twice in the collision hash, logged with "DXDOUBLE:". Run under the
// sanitizer build (ASan).
//
// UActor::SetLocation (UActor_Phys.cpp:181-221) takes the actor out of the
// hash, runs the encroachment check's events (EncroachingOn, Touch, UnTouch,
// EncroachedBy; and Detach/BaseChange of what stands on it) and files it
// again. CollisionSystem::AddToCollision (CollisionSystem.cpp:176) files
// without asking whether the actor is filed already, and keeps only the last
// filing's cells. So an event in that window that files the actor (its own
// SetCollision, Move, SetCollisionSize, SetLocation) leaves it in the cells of
// its old place for good; Destroy removes the last filing's cells alone, and
// the old cells keep a pointer to the destroyed actor, read by every query
// of those cells (VisitActorsInBox: actor->Collision.CheckCounter) once the
// collector has freed it.
//
// On 01_NYC_UNATCOIsland, 3 s in: a DoubleFileToucher (below; its Touch calls
// SetCollision(true, false, false) on itself) spawned 200 units ahead of the
// player, then SetLocation onto the player (its touch of the player is
// made in the check, and its Touch files it at its old place); then it is
// destroyed, and for 300 ticks a TraceActors and a Trace with an extent are
// made through its old place each tick, "obj garbage" asked at tick 10.
// Expected under ASan: heap-use-after-free in VisitActorsInBox once the
// toucher is freed. The original: the same double link (its RemoveActor
// walks the actor's current place), so needs a reading only for whether it
// crashes; the guard is the safety class either way.
//
// The toucher class goes in the dxcap package beside this console:
//
//   class DoubleFileToucher extends Actor;
//   event Touch(Actor Other) { SetCollision(true, false, false); }
//   defaultproperties { bCollideActors=True CollisionRadius=20 CollisionHeight=20 bHidden=True }
//=============================================================================
class DoubleFileConsole extends Console;

var float MapTime;
var int Phase, Ticks;
var string CurrentMap;
var Actor Toucher;
var vector OldPlace;

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor A;
	local vector HitLocation, HitNormal;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0 && MapTime > 3.0)
	{
		OldPlace = P.Location + vector(P.Rotation) * 200;
		Toucher = P.Spawn(class'DoubleFileToucher',,, OldPlace);
		Log("DXDOUBLE: toucher " $ Toucher $ " at " $ OldPlace);
		if (Toucher != None)
		{
			Toucher.SetCollision(false, false, false);
			Toucher.SetCollision(true, false, false);
			Log("DXDOUBLE: SetLocation onto the player " $ Toucher.SetLocation(P.Location));
			Toucher.Destroy();
		}
		Phase = 1;
	}
	else if (Phase == 1)
	{
		foreach P.TraceActors(class'Actor', A, HitLocation, HitNormal, OldPlace + vect(0,0,-100), OldPlace + vect(0,0,100))
			Log("DXDOUBLE: tick " $ Ticks $ " traced " $ A);
		P.Trace(HitLocation, HitNormal, OldPlace + vect(0,0,-100), OldPlace + vect(0,0,100), true, vect(10,10,10));
		if (Ticks == 10)
			P.ConsoleCommand("obj garbage");
		if (++Ticks >= 300)
		{
			Log("DXDOUBLE: done");
			P.ConsoleCommand("exit");
			Phase = 2;
		}
	}
}
