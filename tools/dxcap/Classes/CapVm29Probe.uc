//=============================================================================
// CapVm29Probe: Vm29Console's actor. RunChecks: a static array argument
// before a later one that changes it (FirstOf(Arr, BumpArr(Arr))), an out
// static array set whole (SetAll), an out argument of a call held off
// before a later one that changes it -- a singular one while Held runs, and
// Probe4, named after a probe, while it is disabled --, then enabled (UCC
// takes no out bool: "Booleans may not be out parameters"). State B's
// BeginState goes to C, whose BeginState logs, then to no state; B's label
// Here logs. Each line starts "DXVM29:".
//=============================================================================
class CapVm29Probe extends Actor;

var int Arr[3];

function Say(string S)
{
	Log("DXVM29: " $ S);
}

function int SweepBump(out int X)
{
	X = 10;
	return 0;
}

function int FirstOf(int A[3], int J)
{
	return A[0];
}

function int BumpArr(out int A[3])
{
	A[0] = 10;
	return 0;
}

function SetAll(out int A[3])
{
	A[0] = 7;
	A[1] = 8;
	A[2] = 9;
}

// Held off while Held runs, singular as they are
singular function HeldSingular(out int X, int J)
{
	X = 99;
}

singular function Held()
{
	local int I;

	I = 1;
	HeldSingular(I, SweepBump(I));
	Say("singular held off: I " $ I);
}

// Named after a probe: held off while the probe is disabled
function Probe4(out int X, int J)
{
	X = 99;
}

function RunChecks()
{
	local int I;
	local int L[3];

	Arr[0] = 1;
	Arr[1] = 2;
	Arr[2] = 3;
	Say("static array as reached: " $ FirstOf(Arr, BumpArr(Arr)));
	L[0] = 1;
	L[1] = 2;
	L[2] = 3;
	Say("local static array as reached: " $ FirstOf(L, BumpArr(L)));
	L[0] = 1;
	L[1] = 2;
	L[2] = 3;
	SetAll(L);
	Say("out static array: " $ L[0] $ "," $ L[1] $ "," $ L[2]);
	Held();
	Disable('Probe4');
	I = 1;
	Probe4(I, SweepBump(I));
	Say("disabled probe held off: I " $ I);
	Enable('Probe4');
	I = 1;
	Probe4(I, SweepBump(I));
	Say("probe enabled: I " $ I);
}

state B
{
	function BeginState()
	{
		Say("B's BeginState: to C, then to no state");
		GotoState('C');
		GotoState('');
	}

Here:
	Say("B's label Here");
}

state C
{
	function BeginState()
	{
		Say("C's BeginState");
	}
}

defaultproperties
{
	bHidden=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
}
