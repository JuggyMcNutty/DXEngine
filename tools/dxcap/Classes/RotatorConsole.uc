//=============================================================================
// RotatorConsole: vector-to-rotator conversions, logged with "DXROT:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: rotator(v) for a fixed set of
// vectors -- the axes and their diagonals, every combination of -3..3 on
// each axis, directions every 15 degrees of yaw and pitch, and vectors close
// to where an angle crosses a whole unit -- each logged as its parts read as
// ints and as the rotator's string. Then vector(r), GetAxes and GetUnAxes
// for a few rotators, and a vector turned by each (>>) and back (<<); a
// rotator scaled in place (*=), floats a hair apart and rotators a whole
// turn apart compared (== and !=), floats past an int's range made ints
// and bytes (and stored in int and byte variables, the VM's typed path) and
// rotators scaled past it, Normal near its cutoff, Chr(0) and
// IsInState of a state a state extends,
// and an enum's first and last names (GetEnum); and an exit. What is compared between the engines:
// every line, exactly.
//=============================================================================
class RotatorConsole extends Console;

var float RunTime;
var bool bDone;

function LogOne(vector V)
{
	local rotator R;
	R = rotator(V);
	Log("DXROT: " $ V.X $ "," $ V.Y $ "," $ V.Z $ " -> " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " = " $ string(R));
}

function LogBack(rotator R)
{
	local vector V, X, Y, Z;
	V = vector(R);
	Log("DXROT: back " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " -> " $ V.X $ "," $ V.Y $ "," $ V.Z);
	GetAxes(R, X, Y, Z);
	Log("DXROT: axes " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " -> " $ X $ " " $ Y $ " " $ Z);
	GetUnAxes(R, X, Y, Z);
	Log("DXROT: unaxes " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " -> " $ X $ " " $ Y $ " " $ Z);
	V = vect(100, 20, -30);
	Log("DXROT: turned " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " -> " $ (V >> R) $ " back " $ (V << R));
}

function Operators()
{
	local rotator R, S, T;
	local float A, B;
	local float F[8];
	local int I0, I1, I2, I3, I4, I5;
	local byte B6, B7;
	local CapDoomed D;

	R = rot(1000, -2001, 3003);
	S = (R *= 1.5);
	Log("DXROT: scaled " $ R $ " returned " $ S);
	A = 0.001;
	B = A + 0.00000001;
	Log("DXROT: floats == " $ (A == B) $ " != " $ (A != B) $ "; == itself " $ (A == A));
	R = rot(0, -1, 0);
	S = rot(0, 65535, 0);
	T = rot(65536, 0, -65536);
	F[0] = 3000000000.0;
	F[1] = -3000000000.0;
	F[2] = 5000000000.0;
	F[3] = 10000000000.0 * 10000000000.0;
	F[4] = -F[3];
	F[5] = Sqrt(-1.0);
	F[6] = 300.7;
	F[7] = -1.5;
	Log("DXROT: ints " $ int(F[0]) $ " " $ int(F[1]) $ " " $ int(F[2]) $ " " $ int(F[3]) $ " " $ int(F[4]) $ " " $ int(F[5]) $ "; bytes " $ byte(F[6]) $ " " $ byte(F[7]));
	// The same stored in int and byte variables: the VM's typed path.
	I0 = int(F[0]); I1 = int(F[1]); I2 = int(F[2]); I3 = int(F[3]); I4 = int(F[4]); I5 = int(F[5]);
	B6 = byte(F[6]); B7 = byte(F[7]);
	Log("DXROT: typed ints " $ I0 $ " " $ I1 $ " " $ I2 $ " " $ I3 $ " " $ I4 $ " " $ I5 $ "; bytes " $ B6 $ " " $ B7);
	R = rot(1000, -2001, 3003) * 3000000.0;
	S = rot(16777217, 0, -16777217) * 1.0;
	T = rot(7, 0, 0) / 0.000001;
	Log("DXROT: scaled past " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ "; exact " $ S.Pitch $ " " $ S.Roll $ "; divided " $ T.Pitch);
	Log("DXROT: normals " $ Normal(vect(0.00009, 0, 0)) $ " " $ Normal(vect(0.0001, 0, 0)) $ " " $ Normal(vect(0.00000005, 0.0002, 0)) $ " " $ Normal(vect(1, 2, 3)));
	Log("DXROT: chr " $ Len(Chr(0)) $ " " $ Len("a" $ Chr(0) $ "b") $ " " $ Asc(Chr(0)) $ " " $ Len(Chr(65536)));
	D = Viewport.Actor.Spawn(class'CapDoomed',, 'State', Viewport.Actor.Location + vect(0,0,200));
	D.GotoState('Lingering');
	Log("DXROT: in state " $ D.IsInState('Lingering') $ " " $ D.IsInState('Waiting') $ " " $ D.IsInState('Other'));
	D.Destroy();
	Log("DXROT: rotators a turn apart == " $ (R == S) $ " != " $ (R != S) $ "; zero == " $ (T == rot(0,0,0)) $ " != " $ (T != rot(0,0,0)) $ "; == itself " $ (R == R));
	Log("DXROT: enum " $ GetEnum(enum'ENetRole', 0) $ " " $ GetEnum(enum'ENetRole', 4) $ " " $ GetEnum(enum'ENetRole', 5) $ " " $ GetEnum(enum'ENetRole', -1));
}

function Probes()
{
	local int x, y, z, i, j;
	local vector V;
	local rotator R;

	for (x = -3; x <= 3; x++)
		for (y = -3; y <= 3; y++)
			for (z = -3; z <= 3; z++)
			{
				V.X = x;
				V.Y = y;
				V.Z = z;
				LogOne(V);
			}
	for (i = -12; i <= 12; i++)
		for (j = -6; j <= 6; j++)
		{
			R.Yaw = i * 2730;
			R.Pitch = j * 2730;
			R.Roll = 0;
			LogOne(vector(R) * 100);
		}
	// Near whole units: an angle a hair either side of 1, 2 and 3 units.
	for (i = 1; i <= 3; i++)
	{
		V.X = 1000;
		V.Y = 1000 * Tan(i * 2 * Pi / 65536.0) * 0.999;
		V.Z = 0;
		LogOne(V);
		V.Y = 1000 * Tan(i * 2 * Pi / 65536.0) * 1.001;
		LogOne(V);
		V.Y = -V.Y;
		LogOne(V);
	}
	LogBack(rot(0, 0, 0));
	LogBack(rot(-5691, 20051, 0));
	LogBack(rot(59845, 20051, 0));
	LogBack(rot(16384, 0, 0));
	LogBack(rot(-16384, 32768, 0));
	LogBack(rot(70000, -70000, 1234));
	LogBack(rot(1000, 2001, 3003));
	LogBack(rot(-12345, 23456, -4567));
	Operators();
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes();
		Log("DXROT: done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
