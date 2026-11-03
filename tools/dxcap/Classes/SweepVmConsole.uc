//=============================================================================
// SweepVmConsole: VM edges logged with
// "DXSWEEPVM:". 2 s in, from wherever the run starts: each check below in
// the console's own Tick, then a CapSweepVmProbe spawned, which runs the
// same checks in its own Tick, in the level's tick; 1 s later an exit.
// Every line is compared between the engines.
//
// - Operand order: an operand that is a variable, read after a later
//   operand changes it (the original's natives and calls copy each argument
//   as they step it: Core.dll execLocalVariable 0x1012ebc0, P_GET_*).
// - bool(string): the original's execStringToBool (0x10132490) takes a
//   number by appAtoi.
// - A struct's ==: the original's execStructCmpEq (0x101303a0) compares a
//   string member by UStrProperty::Identical (0x101676f0), appStricmp.
// - Property text: an enum byte from a number (UByteProperty::ImportText,
//   0x10165aa0, reads a token and looks it up as a name only), an int from
//   "+5" (0x10165f00 takes '-' or a digit first), a bool from a token
//   (0x10166380, ReadToken first), and GetPropertyText of a name PlayerPawn
//   declares again over Actor's, bSinglePlayer (0x1013e0b0: own fields first).
// - VSize and Dot to their last bit: (F - 1) * 2^23 of results in [1, 2).
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

function int Mantissa(float F)
{
	return int((F - 1.0) * 8388608.0);
}

function Checks(string Where)
{
	local int I;
	local SweepPair A, B;
	local vector V, W;

	// Operand order
	I = 1;
	Say(Where $ " I + SweepBump(I), I 1: " $ (I + SweepBump(I)));
	I = 1;
	Say(Where $ " Pair(I, SweepBump(I)), I 1: " $ Pair(I, SweepBump(I)));
	Counter = 1;
	Say(Where $ " Counter + BumpCounter(), Counter 1: " $ (Counter + BumpCounter()));
	Text = "a";
	Say(Where $ " Text $ AppendText(), Text a: " $ (Text $ AppendText()));

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
