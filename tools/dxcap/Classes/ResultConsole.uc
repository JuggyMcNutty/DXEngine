//=============================================================================
// ResultConsole: where a script function's result lands when the function
// ends without a value, logged with "DXRESULT:" from wherever the run starts
// (docs/DEVELOPMENT.md, scripted runs). Every function that has a return
// value can run off its end, UCC taking no bare return in one. 2 s in, a
// CapResultProbe 100 units over the player, whose functions of each type run
// off their end: it runs its Results -- each such function's result into a
// variable set before, by every way a result is taken (CapResultProbe's
// header) -- then the console's own lets through it, P.IntOf() and the rest
// into locals and into the probe's variables, and P.OutSet(I) into I, its
// out parameter, and it goes to its state Coding, whose code's lets run in
// its next tick; 1 s later an exit. What is compared between the engines:
// every line, exactly -- but a native's out parameter assigned its own
// result ("int ++ 6"), which the original's post-increment and decrement
// natives write before they step the variable (bugs.md VM.15).
//=============================================================================
class ResultConsole extends Console;

var float RunTime;
var int Phase;
var CapResultProbe Probe;

function Say(string S)
{
	Log("DXRESULT: " $ S);
}

// Each through the probe, into a variable set before
function ContextLets(CapResultProbe P)
{
	local int I;
	local bool B;
	local string S;
	local vector V;
	local Actor A;

	I = 5;
	I = P.IntOf();
	B = True;
	B = P.BoolOf();
	S = "kept";
	S = P.StrOf();
	V = vect(1, 2, 3);
	V = P.VecOf();
	A = P;
	A = P.ActorOf();
	P.HeldInt = 5;
	P.HeldInt = P.IntOf();
	P.bHeld = True;
	P.bHeld = P.BoolOf();
	Say(P.Kind $ ": through the probe: int " $ I $ ", bool " $ B $ ", string [" $ S $ "], vector " $ V $ ", actor kept " $ (A == P) $ ", its int " $ P.HeldInt $ ", its bool " $ P.bHeld);
	I = 5;
	I = P.OutSet(I);
	P.HeldInt = 5;
	P.HeldInt = P.OutSet(P.HeldInt);
	Say(P.Kind $ ": through the probe: out parameter assigned the result: int " $ I $ ", its int " $ P.HeldInt);
}

event Tick(float Delta)
{
	local PlayerPawn Player;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	Player = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Probe = Player.Spawn(class'CapResultProbe',,, Player.Location + vect(0, 0, 100));
		Say("probe made " $ (Probe != None));
		if (Probe != None)
		{
			Probe.Results();
			ContextLets(Probe);
			Probe.GotoState('Coding');
		}
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		if (Probe != None)
			Probe.Destroy();
		Say("done, exiting");
		Player.ConsoleCommand("exit");
		Phase = 9;
	}
}
