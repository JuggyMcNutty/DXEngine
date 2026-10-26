//=============================================================================
// AimConsole: PickTarget and PickAnyTarget, the natives auto-aim asks,
// logged with "DXAIM:" (docs/DEVELOPMENT.md, scripted runs). From wherever
// the run starts, 2 s in, the shot's start the player's place and its
// direction +X exactly, so mirrored targets aim exactly alike: two rats
// spawned 300 units along it, 40 to either side, and PickTarget asked at
// a best aim of 0.93 (the original takes a strictly better aim, so the
// first pawn of the level's pawn list, the rat spawned last); the first rat
// made no projectile target and PickTarget asked again (the original passes
// it over); the second rat alone, moved 2200 units out when there is room
// (past the original's 2000), PickTarget asked at 0.5; then two crates
// spawned over the rats, projectile targets, and PickAnyTarget asked at 0.93
// (the original takes the first of the level's list, no pawn among them).
// Then an exit. What is compared between the engines: every line.
//=============================================================================
class AimConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXAIM: " $ S);
}

function string NameOf(Actor A)
{
	if (A == None)
		return "None";
	return string(A.Name);
}

function Probes(PlayerPawn P)
{
	local Pawn First, Second, Picked;
	local Actor Box1, Box2, Any;
	local vector Dir, Base;
	local float bestAim, bestDist;
	local bool Moved;

	Dir = vect(1, 0, 0);
	Base = P.Location + Dir * 300;
	First = P.Spawn(class'Rat',,, Base - vect(0, 40, 0));
	Second = P.Spawn(class'Rat',,, Base + vect(0, 40, 0));
	Say("rats " $ NameOf(First) $ " and " $ NameOf(Second) $ " 300 along +X, 40 to either side; the pawn list starts with " $ NameOf(P.Level.PawnList));
	if (First == None || Second == None)
		return;

	bestAim = 0.93;
	bestDist = 0;
	Picked = P.PickTarget(bestAim, bestDist, Dir, P.Location);
	Say("PickTarget at 0.93, equal aims: " $ NameOf(Picked) $ ", aim " $ bestAim $ ", distance " $ bestDist);

	First.bProjTarget = False;
	bestAim = 0.93;
	bestDist = 0;
	Picked = P.PickTarget(bestAim, bestDist, Dir, P.Location);
	Say("the first rat no projectile target: " $ NameOf(Picked) $ ", aim " $ bestAim);

	First.bProjTarget = True;
	Second.bProjTarget = False;
	Moved = First.SetLocation(P.Location + Dir * 2200);
	Say("the first rat alone, moved 2200 out: " $ Moved);
	bestAim = 0.5;
	bestDist = 0;
	Picked = P.PickTarget(bestAim, bestDist, Dir, P.Location);
	Say("PickTarget at 0.5: " $ NameOf(Picked) $ ", distance " $ bestDist);
	if (Moved)
		First.SetLocation(Base - vect(0, 40, 0));
	Second.bProjTarget = True;

	Box1 = P.Spawn(class'CrateUnbreakableSmall',,, Base - vect(0, 40, 0) + vect(0, 0, 60));
	Box2 = P.Spawn(class'CrateUnbreakableSmall',,, Base + vect(0, 40, 0) + vect(0, 0, 60));
	Say("crates " $ NameOf(Box1) $ " and " $ NameOf(Box2) $ " over the rats");
	if (Box1 == None || Box2 == None)
		return;
	Box1.bProjTarget = True;
	Box2.bProjTarget = True;
	bestAim = 0.93;
	bestDist = 0;
	Any = P.PickAnyTarget(bestAim, bestDist, Dir, P.Location);
	Say("PickAnyTarget at 0.93, two crates of equal aim and two rats: " $ NameOf(Any) $ ", aim " $ bestAim $ ", distance " $ bestDist);
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes(Viewport.Actor);
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
