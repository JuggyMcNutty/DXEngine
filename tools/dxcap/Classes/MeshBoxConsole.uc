//=============================================================================
// MeshBoxConsole (bugs.md DX.9): GetBoundingBox of actors that are no
// brushes, logged with "DXBOX:" in front, in both engines. On Liberty
// Island, 3 s in, each box asked at one test place and rotation, so that it
// rests on the actor's mesh, scale, pivot and animation alone, never on where
// its physics has left it, plain and with bExact:
//   the level's first 12 decorations as they are;
//   the first of them again at DrawScale 2.5, and as bParticles;
//   the first 3 ScriptedPawns with their own sequence at frames -0.5, 0,
//   0.37 and 0.999, and with a sequence that is none of the mesh's;
//   the player;
//   the first Light and the first PathNode (no mesh: the cylinder's box).
// Then exits.
//=============================================================================
class MeshBoxConsole extends Console;

var float RunTime;
var bool bDone;

function Say(Actor A, string What)
{
	local vector Lo, Hi;
	local bool bOk;

	bOk = A.GetBoundingBox(Lo, Hi, False, vect(100,200,300), rot(1000,2000,3000));
	Log("DXBOX: " $ What $ " " $ A.Class.Name $ " mesh " $ A.Mesh $ " seq " $ A.AnimSequence $ " frame " $ A.AnimFrame
		$ " scale " $ A.DrawScale $ " radius " $ A.CollisionRadius $ " height " $ A.CollisionHeight);
	Log("DXBOX: " $ What $ " box " $ bOk $ " " $ Lo $ " " $ Hi);
	bOk = A.GetBoundingBox(Lo, Hi, True, vect(100,200,300), rot(1000,2000,3000));
	Log("DXBOX: " $ What $ " exact " $ bOk $ " " $ Lo $ " " $ Hi);
	bOk = A.GetBoundingBox(Lo, Hi, False, vect(-7,3,11), rot(0,0,0));
	Log("DXBOX: " $ What $ " unturned " $ bOk $ " " $ Lo $ " " $ Hi);
}

function Frames(ScriptedPawn P, int n)
{
	local name Seq;
	local float Frame;

	Seq = P.AnimSequence;
	Frame = P.AnimFrame;
	P.AnimFrame = -0.5;
	Say(P, "pawn " $ n $ " frame -0.5");
	P.AnimFrame = 0.0;
	Say(P, "pawn " $ n $ " frame 0");
	P.AnimFrame = 0.37;
	Say(P, "pawn " $ n $ " frame 0.37");
	P.AnimFrame = 0.999;
	Say(P, "pawn " $ n $ " frame 0.999");
	P.AnimSequence = 'DXNoSuchSequence';
	Say(P, "pawn " $ n $ " no sequence");
	P.AnimSequence = Seq;
	P.AnimFrame = Frame;
}

event Tick(float Delta)
{
	local PlayerPawn Pawn;
	local Decoration D, First;
	local ScriptedPawn S;
	local Light L;
	local PathNode Node;
	local int n;
	local float Scale;

	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	Pawn = Viewport.Actor;
	RunTime += Delta;
	if (RunTime < 3.0)
		return;
	bDone = True;

	foreach Pawn.AllActors(class'Decoration', D)
	{
		if (First == None)
			First = D;
		Say(D, "decoration " $ n);
		n++;
		if (n >= 12)
			break;
	}
	if (First != None)
	{
		Scale = First.DrawScale;
		First.DrawScale = 2.5;
		Say(First, "scaled");
		First.DrawScale = Scale;
		First.bParticles = True;
		Say(First, "particles");
		First.bParticles = False;
	}
	n = 0;
	foreach Pawn.AllActors(class'ScriptedPawn', S)
	{
		Frames(S, n);
		n++;
		if (n >= 3)
			break;
	}
	Say(Pawn, "player");
	foreach Pawn.AllActors(class'Light', L)
	{
		Say(L, "light");
		break;
	}
	foreach Pawn.AllActors(class'PathNode', Node)
	{
		Say(Node, "pathnode");
		break;
	}
	Log("DXBOX: done, exiting");
	Pawn.ConsoleCommand("exit");
}
