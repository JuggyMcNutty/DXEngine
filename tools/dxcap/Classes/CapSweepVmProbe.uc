//=============================================================================
// CapSweepVmProbe: SweepVmConsole's actor, whose own Tick, in the
// level's tick, runs the console's operand-order, bool(string), struct ==,
// VSize, Dot, Lerp and rotator-by-float checks once, logged with
// "DXSWEEPVM:".
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

function AddInto(out int X, int Y)
{
	X = X + Y;
}

function SweepPair BumpPair(out SweepPair P)
{
	local SweepPair Old;

	Old = P;
	P.N = 10;
	return Old;
}

function int Mant(float F)
{
	if (F < 0)
		F = -F;
	if (F == 0)
		return 0;
	while (F < 1.0)
		F *= 2.0;
	while (F >= 2.0)
		F *= 0.5;
	return int((F - 1.0) * 8388608.0);
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
	local string R;
	local rotator Angles;
	local vector N;

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
	I = 1;
	Say("level's tick: I - SweepBump(I), I 1: " $ (I - SweepBump(I)));
	I = 1;
	Say("level's tick: I - byte(SweepBump(I)), I 1: " $ (I - byte(SweepBump(I))));
	I = 1;
	AddInto(I, SweepBump(I));
	Say("level's tick: AddInto(I, SweepBump(I)), I 1: " $ I);
	I = 1;
	Say("level's tick: I += SweepBump(I), I 1: " $ (I += SweepBump(I)));
	A.S = "abc"; A.N = 1;
	Say("level's tick: A == BumpPair(A), A.N 1: " $ (A == BumpPair(A)));
	I = 1;
	switch (I)
	{
		case SweepBump(I):
			R = "0";
			break;
		case 10:
			R = "10";
			break;
		case 1:
			R = "1";
			break;
		default:
			R = "none";
	}
	Say("level's tick: switch (I), case SweepBump(I) first, I 1: " $ R);
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
	Say("level's tick: Lerp " $ Mantissa(Lerp(0.1273, 1.6448, 1.9779)));
	N = Normal(vect(0.1616, 0.5555, 0.4108));
	Say("level's tick: Normal a " $ Mant(N.X) $ " " $ Mant(N.Y) $ " " $ Mant(N.Z));
	N = Normal(vect(0.4686, 0.1594, 0.1771));
	Say("level's tick: Normal b " $ Mant(N.X) $ " " $ Mant(N.Y) $ " " $ Mant(N.Z));
	Angles = rot(10, 20, 30) * 0.7;
	Say("level's tick: rot * 0.7 " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Angles = 0.7 * rot(10, 20, 30);
	Say("level's tick: 0.7 * rot " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Angles = rot(10, 20, 30);
	Angles *= 0.9;
	Say("level's tick: rot *= 0.9 " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Angles = rot(90, 180, 270) / 90.0;
	Say("level's tick: rot / 90 " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Angles = rot(90, 180, 270);
	Angles /= 90.0;
	Say("level's tick: rot /= 90 " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Destroy();
}
