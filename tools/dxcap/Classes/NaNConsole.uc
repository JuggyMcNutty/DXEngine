//=============================================================================
// NaNConsole: a NaN compared, logged with "DXNAN:" (bugs.md VM.30). 2 s in,
// from wherever the run starts, a NaN made as the square root of -1, N, is
// compared with 0 and with itself by each operator, both as a value and as
// a branch (Branches); then bool of it and of vectors holding it, a vector's,
// a Plane's and a Coords' == and !=, FMax, FMin, FClamp, Abs, Normal, int
// of it and a float switch on it; then an exit.
//
// The original's script compares (Core.dll 0x101352a0 to 0x101357e0) test
// the x87's flags after an fcomp, C3 for equal and C0 for under, both set
// by a NaN on either side: ==, <, <= and ~= are true with a NaN, !=, > and
// >= false. bool of a float (0x10131be0) and of a vector (0x10136cd0) test
// C3 against 0: a NaN is False, a NaN axis 0. A vector's == and !=
// (0x10138430, 0x10138530) test each axis's C3; a struct's == takes each
// member by its property's Identical (0x101690d0), a float's C3
// (0x10166780). Abs (0x10135c60) negates where C0 says under 0, so a NaN's
// sign turns over and -0 stays -0; FMax (0x10136400) takes B where C0 says
// A is under it; Normal's SafeNormal (0x10144be0) is zero where C0 says the
// squares' sum is under 1e-8. A float switch matches a case by its bits
// (0x1012f850): no case takes the NaN. The NaN is the x87's indefinite,
// 0xFFC00000, printed -1.#IND00, as an x86-64 build's square root makes it
// too; an aarch64 build's makes 0x7FC00000, 1.#QNAN0.
//
// So the original's lines read: N -1.#IND00, -N 1.#QNAN0, int(N) 0; == True
// three times, != False twice; < and <= True, > and >= False, each both
// ways; ~= True, True, and False for 1 ~= 0; the branches of N against 0,
// of 0 against N and of N against itself " == < <= ~=", of 1 against 0
// " != > >=", of 0 against 0 " == <= >= ~="; bool(N) and bool(-N) False,
// bool(1) True, if bool(N) not taken; bool(N,0,0) False, bool(N,1,0) True,
// bool(N,N,N) False, not taken; (N,1,2) == (5,1,2) True, != False, and with
// a Z apart False, True; the Plane and the Coords alike; FMax(N,1) 1.000000,
// FMax(1,N) -1.#IND00, FMin(N,1) -1.#IND00, FMin(1,N) 1.000000, FClamp(N,1,5)
// 1.000000; Abs(N) 1.#QNAN0, Abs(-N) -1.#IND00, Abs(-0) -0.000000, Abs(-1)
// 1.000000; Normal(N,0,0) 0.000000,0.000000,0.000000; switch default.
//=============================================================================
class NaNConsole extends Console;

var float RunTime;
var bool bDone;

// The operators whose branch A against B takes, each its own if
function string Branches(float A, float B)
{
	local string S;

	if (A == B)
		S = S $ " ==";
	if (A != B)
		S = S $ " !=";
	if (A < B)
		S = S $ " <";
	if (A <= B)
		S = S $ " <=";
	if (A > B)
		S = S $ " >";
	if (A >= B)
		S = S $ " >=";
	if (A ~= B)
		S = S $ " ~=";
	if (S == "")
		S = " none";
	return S;
}

function Run()
{
	local float N, Z, One, Five, MinusOne;
	local vector V, W;
	local Plane P1, P2;
	local Coords C1, C2;
	local string S;

	MinusOne = -1.0;
	Z = 0.0;
	One = 1.0;
	Five = 5.0;
	N = Sqrt(MinusOne);
	Log("DXNAN: N " $ N $ ", -N " $ (-N) $ ", int(N) " $ int(N));

	// As values, each operator's native
	Log("DXNAN: N == 0 " $ (N == Z) $ ", 0 == N " $ (Z == N) $ ", N == N " $ (N == N)
		$ ", N != 0 " $ (N != Z) $ ", N != N " $ (N != N));
	Log("DXNAN: N < 0 " $ (N < Z) $ ", 0 < N " $ (Z < N) $ ", N <= 0 " $ (N <= Z) $ ", 0 <= N " $ (Z <= N));
	Log("DXNAN: N > 0 " $ (N > Z) $ ", 0 > N " $ (Z > N) $ ", N >= 0 " $ (N >= Z) $ ", 0 >= N " $ (Z >= N));
	Log("DXNAN: N ~= 0 " $ (N ~= Z) $ ", N ~= N " $ (N ~= N) $ ", 1 ~= 0 " $ (One ~= Z));

	// As branches
	Log("DXNAN: branches N, 0:" $ Branches(N, Z));
	Log("DXNAN: branches 0, N:" $ Branches(Z, N));
	Log("DXNAN: branches N, N:" $ Branches(N, N));
	Log("DXNAN: branches 1, 0:" $ Branches(One, Z));
	Log("DXNAN: branches 0, 0:" $ Branches(Z, Z));

	// bool of a float and of a vector, as a value and as a branch
	S = "";
	if (bool(N))
		S = " taken";
	else
		S = " not taken";
	Log("DXNAN: bool(N) " $ bool(N) $ ", bool(-N) " $ bool(-N) $ ", bool(1) " $ bool(One) $ "; if bool(N)" $ S);
	V.X = N;
	V.Y = Z;
	V.Z = Z;
	S = "DXNAN: bool(N,0,0) " $ bool(V);
	V.Y = One;
	S = S $ ", bool(N,1,0) " $ bool(V);
	V.Y = N;
	V.Z = N;
	S = S $ ", bool(N,N,N) " $ bool(V);
	if (bool(V))
		S = S $ "; if bool(N,N,N) taken";
	else
		S = S $ "; if bool(N,N,N) not taken";
	Log(S);

	// A vector's == and !=, each axis
	V.X = N;
	V.Y = 1.0;
	V.Z = 2.0;
	W.X = Five;
	W.Y = 1.0;
	W.Z = 2.0;
	S = "DXNAN: (N,1,2) == (5,1,2) " $ (V == W) $ ", != " $ (V != W);
	V.Z = 3.0;
	S = S $ "; (N,1,3) == (5,1,2) " $ (V == W) $ ", != " $ (V != W);
	Log(S);

	// The natives that compare
	Log("DXNAN: FMax(N,1) " $ FMax(N, One) $ ", FMax(1,N) " $ FMax(One, N) $ ", FMin(N,1) " $ FMin(N, One)
		$ ", FMin(1,N) " $ FMin(One, N) $ ", FClamp(N,1,5) " $ FClamp(N, One, Five));
	Log("DXNAN: Abs(N) " $ Abs(N) $ ", Abs(-N) " $ Abs(-N) $ ", Abs(-0) " $ Abs(-Z) $ ", Abs(-1) " $ Abs(MinusOne));
	V.X = N;
	V.Y = Z;
	V.Z = Z;
	Log("DXNAN: Normal(N,0,0) " $ Normal(V));

	// A float switch: a case by its bits
	switch (N)
	{
	case 0.0:
		S = "case 0";
		break;
	case 1.0:
		S = "case 1";
		break;
	default:
		S = "default";
		break;
	}
	Log("DXNAN: switch (N) took " $ S);

	// Last, the original crashing at a Plane's == with a NaN member
	// A struct's == and !=, each member: a Plane, which DXEngine compares
	// through its float properties, and a Coords, which it holds as a value
	// of its own
	P1.X = N;
	P1.Y = 1.0;
	P1.Z = 2.0;
	P1.W = 3.0;
	P2.X = Five;
	P2.Y = 1.0;
	P2.Z = 2.0;
	P2.W = 3.0;
	S = "DXNAN: plane (N,1,2,3) == (5,1,2,3) " $ (P1 == P2) $ ", != " $ (P1 != P2);
	P1.W = 4.0;
	S = S $ "; (N,1,2,4) == (5,1,2,3) " $ (P1 == P2) $ ", != " $ (P1 != P2);
	Log(S);
	C1.Origin.X = N;
	C2.Origin.X = Five;
	S = "DXNAN: coords origin (N,0,0) == (5,0,0) " $ (C1 == C2) $ ", != " $ (C1 != C2);
	C1.XAxis.X = One;
	S = S $ "; with an X axis apart " $ (C1 == C2) $ ", != " $ (C1 != C2);
	Log(S);

}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	Run();
	Log("DXNAN: done, exiting");
	Viewport.Actor.ConsoleCommand("exit");
}
