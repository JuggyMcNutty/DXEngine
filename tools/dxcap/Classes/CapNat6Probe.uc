//=============================================================================
// CapNat6Probe: Nat6Console's vector cases whose last bit rests on the order
// and the precision of the sums: MirrorVectorByNormal by normals off the
// axes, and vectors over floats. Cases() logs each with "DXNAT6:" and where
// it ran: Nat6Console calls it in its own tick, the console's, and the
// probe in its own, the level's, where the original's x87 holds fewer bits.
//=============================================================================
class CapNat6Probe extends Info;

var bool bDone;

function One(string Where, string What, vector M)
{
	Log("DXNAT6: " $ Where $ " " $ What $ " -> " $ M $ " ; e7 " $ (M * 10000000.0));
}

function Cases(string Where)
{
	local vector V;

	One(Where, "Mirror((0.3,0.4,-0.866), (1,2,3))", MirrorVectorByNormal(vect(0.3,0.4,-0.866), vect(1,2,3)));
	One(Where, "Mirror((0.7,0.1,-0.7), (0.57735,0.57735,0.57735))", MirrorVectorByNormal(vect(0.7,0.1,-0.7), vect(0.57735,0.57735,0.57735)));
	One(Where, "Mirror((0.123456,-0.789,0.456), (0.267261,0.534522,0.801784))", MirrorVectorByNormal(vect(0.123456,-0.789,0.456), vect(0.267261,0.534522,0.801784)));
	One(Where, "Mirror((0.9,-0.2,0.31), (-0.3,0.9,0.31))", MirrorVectorByNormal(vect(0.9,-0.2,0.31), vect(-0.3,0.9,0.31)));
	One(Where, "Mirror((0.5,0.25,0.125), (0.6,0,0.8))", MirrorVectorByNormal(vect(0.5,0.25,0.125), vect(0.6,0.0,0.8)));
	One(Where, "Mirror((0.333,0.666,-0.999), (0,0.70710678,0.70710678))", MirrorVectorByNormal(vect(0.333,0.666,-0.999), vect(0.0,0.70710678,0.70710678)));
	One(Where, "Mirror((0.1,0.2,0.3), (0.001,0.002,0.003))", MirrorVectorByNormal(vect(0.1,0.2,0.3), vect(0.001,0.002,0.003)));
	One(Where, "(0.3,0.7,0.9) / 3.0", vect(0.3,0.7,0.9) / 3.0);
	One(Where, "(0.3,0.7,0.9) / 0.7", vect(0.3,0.7,0.9) / 0.7);
	One(Where, "(1.1,0.23,0.37) / 1.3", vect(1.1,0.23,0.37) / 1.3);
	V = vect(1.1,0.23,0.37);
	V /= 7.0;
	One(Where, "(1.1,0.23,0.37) /= 7.0", V);
	V = vect(0.11,1.23,0.037);
	V /= 0.9;
	One(Where, "(0.11,1.23,0.037) /= 0.9", V);
}

event Tick(float DeltaTime)
{
	if (bDone)
		return;
	bDone = true;
	Cases("level's tick:");
}
