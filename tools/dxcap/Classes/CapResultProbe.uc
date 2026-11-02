//=============================================================================
// CapResultProbe: ResultConsole's actor -- a function of each type that ends
// without a value, off its end, and each way its result is taken, every one
// into a variable set before: a Let of each kind of local, of a struct's
// member, an array's element, another's variable and its own; through a
// conversion, a cast and a member; returned from another function, beside a
// None context's member returned; read by operators, a condition, a switch,
// a member and a struct compare; given to a script function's parameter, a
// native's and a native's optional one (Spawn's location, Trace's start);
// with them a call held off (a singular one, a disabled probe's), an out
// parameter that is also the variable assigned, a foreach, a state's
// function and state code. Each line logged with "DXRESULT:" and the probe's
// Kind.
//=============================================================================
class CapResultProbe extends Actor;

struct CapPair
{
	var int N;
	var string S;
	var vector V;
	var bool bOn;
};

var string Kind;
var int Calls;
var int HeldInt;
var bool bHeld;
var string HeldStr;
var Actor HeldActor;
var int HeldArr[3];

function Say(string S)
{
	Log("DXRESULT: " $ Kind $ ": " $ S);
}

// A function of each type that ends without a value, off its end
function int IntOf()
{
	Calls++;
}

function byte ByteOf()
{
	Calls++;
}

function float FloatOf()
{
	Calls++;
}

function bool BoolOf()
{
	Calls++;
}

function name NameOf()
{
	Calls++;
}

function string StrOf()
{
	Calls++;
}

function Actor ActorOf()
{
	Calls++;
}

function class<Actor> ClassOf()
{
	Calls++;
}

function vector VecOf()
{
	Calls++;
}

function rotator RotOf()
{
	Calls++;
}

function CapPair PairOf()
{
	Calls++;
}

// One that ends after a foreach left by a break
function int LoopOf()
{
	local CapResultProbe A;

	Calls++;
	foreach AllActors(class'CapResultProbe', A)
		break;
}

// Each one's result returned
function int IntChain()
{
	return IntOf();
}

function bool BoolChain()
{
	return BoolOf();
}

function string StrChain()
{
	return StrOf();
}

function Actor ActorChain()
{
	return ActorOf();
}

// A None context's member returned, which the original zero-fills; a
// string's, which it fills with nothing; an element through None, which it
// does not copy
function int NoneInt()
{
	local CapResultProbe Q;

	return Q.HeldInt;
}

function string NoneStr()
{
	local CapResultProbe Q;

	return Q.HeldStr;
}

function int NoneElement()
{
	local CapResultProbe Q;

	return Q.HeldArr[1];
}

// A parameter given each one's result
function int EchoInt(int X)
{
	return X;
}

function string EchoStr(string X)
{
	return X;
}

function Actor EchoActor(Actor X)
{
	return X;
}

function vector EchoVec(vector X)
{
	return X;
}

// An out parameter that is the variable the result is assigned to
function int OutSet(out int X)
{
	X = 3;
	return 8;
}

function int OutFell(out int X)
{
	X = 3;
}

// Held off while HeldOff runs, singular as they are
singular function int SingularInt()
{
	return 7;
}

singular function bool SingularBool()
{
	return True;
}

singular function string SingularStr()
{
	return "seven";
}

// Probes: held off while disabled
function Actor SpecialHandling(Pawn Other)
{
	return self;
}

function bool EncroachingOn(Actor Other)
{
	return True;
}

// Into locals of each kind
function Lets()
{
	local int I;
	local byte Bt;
	local float F;
	local bool B;
	local name N;
	local string S;
	local Actor A;
	local class<Actor> C;
	local vector V;
	local rotator R;
	local CapPair Pair;

	I = 5;
	I = IntOf();
	Bt = 9;
	Bt = ByteOf();
	F = 1.5;
	F = FloatOf();
	B = True;
	B = BoolOf();
	N = 'Kept';
	N = NameOf();
	S = "kept";
	S = StrOf();
	A = self;
	A = ActorOf();
	C = class'CapMarker';
	C = ClassOf();
	V = vect(1, 2, 3);
	V = VecOf();
	R = rot(1, 2, 3);
	R = RotOf();
	Pair.N = 7;
	Pair.S = "pair";
	Pair.V = vect(4, 5, 6);
	Pair.bOn = True;
	Pair = PairOf();
	Say("lets: int " $ I $ ", byte " $ Bt $ ", float " $ F $ ", bool " $ B $ ", name " $ N $ ", string [" $ S $ "], actor kept " $ (A == self) $ ", class kept " $ (C == class'CapMarker') $ ", vector " $ V $ ", rotator " $ R $ ", struct " $ Pair.N $ " [" $ Pair.S $ "] " $ Pair.V $ " " $ Pair.bOn);
}

// Into a struct's members, an array's element and another's variables
function Members()
{
	local CapPair Pair;
	local int Arr[3];
	local CapResultProbe Q;

	Pair.N = 7;
	Pair.N = IntOf();
	Pair.S = "pair";
	Pair.S = StrOf();
	Pair.bOn = True;
	Pair.bOn = BoolOf();
	Arr[1] = 5;
	Arr[1] = IntOf();
	Q = self;
	Q.HeldInt = 5;
	Q.HeldInt = IntOf();
	Q.HeldStr = "kept";
	Q.HeldStr = StrOf();
	Q.bHeld = True;
	Q.bHeld = BoolOf();
	Say("members: struct int " $ Pair.N $ ", struct string [" $ Pair.S $ "], struct bool " $ Pair.bOn $ ", element " $ Arr[1] $ ", another's int " $ Q.HeldInt $ ", string [" $ Q.HeldStr $ "], bool " $ Q.bHeld);
}

// Into this probe's own variables
function Instances()
{
	HeldInt = 5;
	HeldInt = IntOf();
	HeldStr = "kept";
	HeldStr = StrOf();
	HeldActor = self;
	HeldActor = ActorOf();
	bHeld = True;
	bHeld = BoolOf();
	HeldArr[1] = 5;
	HeldArr[1] = IntOf();
	Say("own variables: int " $ HeldInt $ ", string [" $ HeldStr $ "], actor kept " $ (HeldActor == self) $ ", bool " $ bHeld $ ", element " $ HeldArr[1]);
}

// Through a conversion, a cast or a member, each of which writes its own value
function Conversions()
{
	local int I;
	local float F;
	local bool B;
	local string S, T;
	local Actor A;
	local CapPair Pair;

	F = 1.5;
	F = IntOf();
	I = 5;
	I = ByteOf();
	B = True;
	B = bool(IntOf());
	A = self;
	A = Pawn(ActorOf());
	S = "kept";
	S = string(IntOf());
	T = "kept";
	T = string(NameOf());
	Pair.N = 7;
	Pair.N = PairOf().N;
	Pair.S = "pair";
	Pair.S = PairOf().S;
	Say("conversions: float " $ F $ ", int " $ I $ ", bool " $ B $ ", cast None " $ (A == None) $ ", string [" $ S $ "] [" $ T $ "], member " $ Pair.N $ " [" $ Pair.S $ "]");
}

// Returned from another function, beside None contexts' members returned
function Chains()
{
	local int I, J, K;
	local bool B;
	local string S, T;
	local Actor A;

	I = 5;
	I = IntChain();
	B = True;
	B = BoolChain();
	S = "kept";
	S = StrChain();
	A = self;
	A = ActorChain();
	J = 5;
	J = NoneInt();
	T = "kept";
	T = NoneStr();
	K = 5;
	K = NoneElement();
	Say("returned: int " $ I $ ", bool " $ B $ ", string [" $ S $ "], actor kept " $ (A == self) $ "; through None: int " $ J $ ", string [" $ T $ "], element " $ K);
}

// Read by operators, a condition, a switch, a member and a struct compare
function Expressions()
{
	local CapPair ZeroPair;

	Say("expressions: " $ (IntOf() + 2) $ " " $ (IntOf() == 0) $ " " $ (FloatOf() * 3.0) $ " " $ (!BoolOf()) $ " " $ (ActorOf() == None) $ " " $ (NameOf() == '') $ " [" $ (StrOf() $ "x") $ "] " $ VecOf().Z $ " " $ PairOf().N $ " [" $ PairOf().S $ "] " $ (PairOf() == ZeroPair));
	if (BoolOf())
		Say("if taken");
	else
		Say("if not taken");
	switch (IntOf())
	{
		case 0: Say("switch int 0"); break;
		case 5: Say("switch int 5"); break;
		default: Say("switch int default"); break;
	}
	switch (StrOf())
	{
		case "": Say("switch string empty"); break;
		case "kept": Say("switch string kept"); break;
		default: Say("switch string default"); break;
	}
	switch (NameOf())
	{
		case 'None': Say("switch name None"); break;
		default: Say("switch name default"); break;
	}
}

// Given to a script function's parameter, which its frame zeroes, and to a
// native's, which the native zeroes
function Arguments()
{
	local int I;
	local string S;
	local Actor A;
	local vector V;

	I = 5;
	I = EchoInt(IntOf());
	S = "kept";
	S = EchoStr(StrOf());
	A = self;
	A = EchoActor(ActorOf());
	V = vect(1, 2, 3);
	V = EchoVec(VecOf());
	Say("script parameters: int " $ I $ ", string [" $ S $ "], actor None " $ (A == None) $ ", vector " $ V);
	Say("native parameters: Max " $ Max(IntOf(), -3) $ ", Len " $ Len(StrOf()) $ ", InStr " $ InStr(StrOf(), "x") $ ", Left [" $ Left(StrOf(), 2) $ "]");
}

// Given to a native's optional parameter, which keeps the native's default:
// Spawn's location the spawner's, Trace's start the tracer's
function Optionals()
{
	local CapMarker M;
	local Actor Hit, Hit2;
	local vector End, HL, HN, HL2, HN2;

	M = Spawn(class'CapMarker',,, VecOf());
	if (M == None)
	{
		Say("optional: spawned None");
	}
	else
	{
		Say("optional: spawned at the probe " $ (M.Location == Location) $ ", at the origin " $ (M.Location == vect(0, 0, 0)));
		M.Destroy();
	}
	End = Location - vect(0, 0, 100000);
	Hit = Trace(HL, HN, End, VecOf());
	Hit2 = Trace(HL2, HN2, End, Location);
	Say("optional: trace as from the probe " $ (Hit == Hit2 && HL == HL2 && HN == HN2));
	Hit2 = Trace(HL2, HN2, End, vect(0, 0, 0));
	Say("optional: trace as from the origin " $ (Hit == Hit2 && HL == HL2 && HN == HN2));
}

// Singular, so that its object's singular functions it calls are held off
singular function HeldOff()
{
	local int I;
	local bool B;
	local string S;

	I = 5;
	I = SingularInt();
	B = True;
	B = SingularBool();
	S = "kept";
	S = SingularStr();
	Say("singular held off: int " $ I $ ", bool " $ B $ ", string [" $ S $ "]");
}

// A probe's function called while the probe is disabled, then enabled
function ProbeCalls()
{
	local Actor A;
	local bool B;

	Disable('SpecialHandling');
	Disable('EncroachingOn');
	A = Level;
	A = SpecialHandling(None);
	B = True;
	B = EncroachingOn(None);
	Say("probes disabled: actor kept " $ (A == Level) $ ", bool " $ B);
	Enable('SpecialHandling');
	Enable('EncroachingOn');
	A = None;
	A = SpecialHandling(None);
	B = False;
	B = EncroachingOn(None);
	Say("probes enabled: actor self " $ (A == self) $ ", bool " $ B);
}

// An out parameter that is the variable assigned: the original writes the
// result, if any, then copies the parameter back over it
function OutAliases()
{
	local int I, J;

	J = 5;
	J = OutFell(J);
	Say("out parameter assigned the result: out 3 and no return leave " $ J);
	I = 5;
	I = OutSet(I);
	Say("out parameter assigned the result: out 3 and return 8 leave " $ I);
}

// After and in a foreach, and a state's function
function Loops()
{
	local CapResultProbe Q;
	local int I, n, k;

	I = 5;
	I = LoopOf();
	Say("ended at a foreach: " $ I);
	foreach AllActors(class'CapResultProbe', Q)
	{
		k++;
		I = 5;
		I = Q.IntOf();
		if (I == 5)
			n++;
	}
	Say("in a foreach: " $ n $ " of " $ k $ " kept");
	GotoState('Overriding');
	I = 5;
	I = IntOf();
	Say("a state's function: " $ I);
	GotoState('');
}

function Results()
{
	Lets();
	Members();
	Instances();
	Conversions();
	Chains();
	Expressions();
	Arguments();
	Optionals();
	HeldOff();
	ProbeCalls();
	OutAliases();
	Loops();
	Say("functions run: " $ Calls);
}

state Overriding
{
	function int IntOf()
	{
		Calls += 2;
	}
}

// Its code's lets, run in the tick after ResultConsole sends it here
state Coding
{
Begin:
	HeldInt = 5;
	HeldInt = IntOf();
	HeldStr = "kept";
	HeldStr = StrOf();
	bHeld = True;
	bHeld = BoolOf();
	Say("state code: int " $ HeldInt $ ", string [" $ HeldStr $ "], bool " $ bHeld);
}

defaultproperties
{
	Kind="off its end"
	bHidden=True
}
