//=============================================================================
// SweepVmConsole: VM edges logged with
// "DXSWEEPVM:". 2 s in, from wherever the run starts: each check below in
// the console's own Tick, then a CapSweepVmProbe spawned, which runs the
// same checks in its own Tick, in the level's tick; 1 s later an exit.
// Every line is compared between the engines.
//
// - Operand order: an operand that is a variable, read after a later
//   operand changes it (the original's natives and calls copy each argument
//   as they step it: Core.dll execLocalVariable 0x1012ebc0, P_GET_*): a
//   native's, a script function's, an instance variable, a string, a fast
//   operator's (-), a typed one's (- of a byte), a script function's out
//   parameter (CallFunction 0x1013f4e0 steps it in as it is), +=, which
//   takes the variable itself (P_GET_INT_REF), a struct's == (0x101303a0)
//   and a switch's condition (0x1012f850).
// - bool(string): the original's execStringToBool (0x10132490) takes a
//   number by appAtoi; int(string) and byte(string) (0x10132320,
//   0x101321b0) too, an out-of-range number, blanks and a sign among them
//   in the console alone.
// - A struct's ==: the original's execStructCmpEq (0x101303a0) compares a
//   string member by UStrProperty::Identical (0x101676f0), appStricmp.
// - Property text: an enum byte from a number (UByteProperty::ImportText,
//   0x10165aa0, reads a token and looks it up as a name only), an int from
//   "+5" (0x10165f00 takes '-' or a digit first), a bool from a token
//   (0x10166380, ReadToken first), and GetPropertyText of a name PlayerPawn
//   declares again over Actor's, bSinglePlayer (0x1013e0b0: own fields first).
// - VSize, Dot and Lerp to their last bit: (F - 1) * 2^23 of results in
//   [1, 2); a rotator times a float and over one, both ways round and in
//   place (each angle by _ftol of the x87's product: Core.dll 0x101395e0,
//   0x101397c0).
//=============================================================================
class SweepVmConsole extends Console;

enum ESweepKind { SK_Zero, SK_One, SK_Two };

struct SweepPair
{
	var string S;
	var int N;
};

var float RunTime;
var int Phase;
var int Counter;
var string Text;
var ESweepKind Kind;
var int IntValue;
var bool BoolValue;

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

function int Mantissa(float F)
{
	return int((F - 1.0) * 8388608.0);
}

function Checks(string Where)
{
	local int I;
	local SweepPair A, B;
	local vector V, W;
	local string R;
	local rotator Angles;

	// Operand order
	I = 1;
	Say(Where $ " I + SweepBump(I), I 1: " $ (I + SweepBump(I)));
	I = 1;
	Say(Where $ " Pair(I, SweepBump(I)), I 1: " $ Pair(I, SweepBump(I)));
	Counter = 1;
	Say(Where $ " Counter + BumpCounter(), Counter 1: " $ (Counter + BumpCounter()));
	Text = "a";
	Say(Where $ " Text $ AppendText(), Text a: " $ (Text $ AppendText()));
	I = 1;
	Say(Where $ " I - SweepBump(I), I 1: " $ (I - SweepBump(I)));
	I = 1;
	Say(Where $ " I - byte(SweepBump(I)), I 1: " $ (I - byte(SweepBump(I))));
	I = 1;
	AddInto(I, SweepBump(I));
	Say(Where $ " AddInto(I, SweepBump(I)), I 1: " $ I);
	I = 1;
	Say(Where $ " I += SweepBump(I), I 1: " $ (I += SweepBump(I)));
	A.S = "abc"; A.N = 1;
	Say(Where $ " A == BumpPair(A), A.N 1: " $ (A == BumpPair(A)));
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
	Say(Where $ " switch (I), case SweepBump(I) first, I 1: " $ R);

	// bool(string)
	Say(Where $ " bool 0.5 " $ bool("0.5") $ ", -0.5 " $ bool("-0.5") $ ", 0x1 " $ bool("0x1") $ ", 1e3 " $ bool("1e3") $ ", 7 " $ bool("7"));

	// A struct's == with a string member in another case
	A.S = "abc"; A.N = 1;
	B.S = "ABC"; B.N = 1;
	Say(Where $ " struct == abc/ABC " $ (A == B) $ ", != " $ (A != B));

	// VSize and Dot to the last bit (each value in [1, 2))
	V = vect(0.4511, 0.8189, 0.8704);
	Say(Where $ " VSize a " $ Mantissa(VSize(V)));
	V = vect(0.6733, 0.2378, 0.8297);
	Say(Where $ " VSize b " $ Mantissa(VSize(V)));
	V = vect(0.8559, 0.8916, 0.8685);
	Say(Where $ " VSize c " $ Mantissa(VSize(V)));
	V = vect(0.7106, 0.4016, 0.8861); W = vect(0.2826, 0.4927, 0.73);
	Say(Where $ " Dot a " $ Mantissa(V dot W));
	V = vect(0.653, 0.8952, 0.7753); W = vect(0.3992, 0.4701, 0.6681);
	Say(Where $ " Dot b " $ Mantissa(V dot W));
	V = vect(0.8559, 0.8916, 0.8685); W = vect(0.4552, 0.3543, 0.3588);
	Say(Where $ " Dot c " $ Mantissa(V dot W));
	Say(Where $ " Lerp " $ Mantissa(Lerp(0.1273, 1.6448, 1.9779)));

	// A rotator times a float and over one
	Angles = rot(10, 20, 30) * 0.7;
	Say(Where $ " rot * 0.7 " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Angles = 0.7 * rot(10, 20, 30);
	Say(Where $ " 0.7 * rot " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Angles = rot(10, 20, 30);
	Angles *= 0.9;
	Say(Where $ " rot *= 0.9 " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Angles = rot(90, 180, 270) / 90.0;
	Say(Where $ " rot / 90 " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
	Angles = rot(90, 180, 270);
	Angles /= 90.0;
	Say(Where $ " rot /= 90 " $ Angles.Pitch $ "," $ Angles.Yaw $ "," $ Angles.Roll);
}

function TextChecks()
{
	local PlayerPawn P;
	local bool bOld;
	local Actor AP;

	// A name a subclass declares again: the original's GetPropertyText finds
	// the class's own first (Core.dll 0x1013e0b0, the field walk 0x10143fd0)
	P = Viewport.Actor;
	AP = P;
	bOld = P.bSinglePlayer;
	Say("shadowed bSinglePlayer: PlayerPawn's " $ P.bSinglePlayer $ ", Actor's " $ AP.bSinglePlayer $ ", GetPropertyText " $ P.GetPropertyText("bSinglePlayer"));
	P.bSinglePlayer = !bOld;
	Say("PlayerPawn's flipped: PlayerPawn's " $ P.bSinglePlayer $ ", Actor's " $ AP.bSinglePlayer $ ", GetPropertyText " $ P.GetPropertyText("bSinglePlayer"));
	P.bSinglePlayer = bOld;

	Kind = SK_Zero;
	SetPropertyText("Kind", "2");
	Say("enum Kind from 2: " $ GetPropertyText("Kind"));
	SetPropertyText("Kind", "SK_One");
	Say("enum Kind from SK_One: " $ GetPropertyText("Kind"));
	SetPropertyText("Kind", "SK_Two junk");
	Say("enum Kind from SK_Two junk: " $ GetPropertyText("Kind"));
	IntValue = 3;
	SetPropertyText("IntValue", "+5");
	Say("int from +5: " $ IntValue);
	BoolValue = False;
	SetPropertyText("BoolValue", "True,");
	Say("bool from True, : " $ BoolValue);
	BoolValue = False;
	SetPropertyText("BoolValue", "1.0");
	Say("bool from 1.0: " $ BoolValue);
	BoolValue = False;
	SetPropertyText("BoolValue", Chr(34) $ "True" $ Chr(34));
	Say("bool from a quoted True: " $ BoolValue);

	// int, byte and bool from text by appAtoi (MSVCRT's _wtoi, Core.dll
	// 0x101235b0): blanks and a sign first, the digits up to the first
	// that is not one, and past an int's range
	Say("appAtoi: int 2147483648 " $ int("2147483648") $ ", -2147483649 " $ int("-2147483649") $ ", 4294967297 " $ int("4294967297") $ ", ' +12x' " $ int(" +12x") $ ", tab 7 " $ int(Chr(9) $ "7") $ ", byte 300 " $ byte("300") $ ", bool 4294967296 " $ bool("4294967296") $ ", ' 5' " $ bool(" 5"));
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Checks("console's tick:");
		TextChecks();
		Viewport.Actor.Spawn(class'CapSweepVmProbe');
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 9;
	}
}
