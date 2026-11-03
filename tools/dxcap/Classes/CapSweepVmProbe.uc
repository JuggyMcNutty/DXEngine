//=============================================================================
// CapSweepVmProbe: SweepVmConsole's actor, whose own Tick, in the
// level's tick, runs the console's operand-order, bool(string), struct ==,
// VSize and Dot checks once, logged with "DXSWEEPVM:".
//=============================================================================
class CapSweepVmProbe extends Info;

struct SweepPair
{
	var string S;
	var int N;
};

var bool bDone;
var int Counter;
var string Text;

function Say(string S)
{
	Log("DXSWEEPVM: " $ S);
}

function int SweepBump(out int X)
{
	X = 10;
	return 0;
}

function int BumpCounter()
{
	Counter = 10;
	return 0;
}

function string AppendText()
{
	Text = "x";
	return "b";
}

function string Pair(int A, int B)
{
	return A $ "," $ B;
}

function int Mantissa(float F)
{
	return int((F - 1.0) * 8388608.0);
}

event Tick(float DeltaTime)
{
	local int I;
	local SweepPair A, B;
	local vector V, W;

	if (bDone)
		return;
	bDone = true;

	I = 1;
	Say("level's tick: I + SweepBump(I), I 1: " $ (I + SweepBump(I)));
	I = 1;
	Say("level's tick: Pair(I, SweepBump(I)), I 1: " $ Pair(I, SweepBump(I)));
	Counter = 1;
	Say("level's tick: Counter + BumpCounter(), Counter 1: " $ (Counter + BumpCounter()));
	Text = "a";
	Say("level's tick: Text $ AppendText(), Text a: " $ (Text $ AppendText()));
	Say("level's tick: bool 0.5 " $ bool("0.5") $ ", -0.5 " $ bool("-0.5") $ ", 0x1 " $ bool("0x1") $ ", 1e3 " $ bool("1e3") $ ", 7 " $ bool("7"));
	A.S = "abc"; A.N = 1;
	B.S = "ABC"; B.N = 1;
	Say("level's tick: struct == abc/ABC " $ (A == B) $ ", != " $ (A != B));
	V = vect(0.4511, 0.8189, 0.8704);
	Say("level's tick: VSize a " $ Mantissa(VSize(V)));
	V = vect(0.6733, 0.2378, 0.8297);
	Say("level's tick: VSize b " $ Mantissa(VSize(V)));
	V = vect(0.8559, 0.8916, 0.8685);
	Say("level's tick: VSize c " $ Mantissa(VSize(V)));
	V = vect(0.7106, 0.4016, 0.8861); W = vect(0.2826, 0.4927, 0.73);
	Say("level's tick: Dot a " $ Mantissa(V dot W));
	V = vect(0.653, 0.8952, 0.7753); W = vect(0.3992, 0.4701, 0.6681);
	Say("level's tick: Dot b " $ Mantissa(V dot W));
	V = vect(0.8559, 0.8916, 0.8685); W = vect(0.4552, 0.3543, 0.3588);
	Say("level's tick: Dot c " $ Mantissa(V dot W));
	Destroy();
}
