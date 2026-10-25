//=============================================================================
// CapNoneProbe: NoneConsole's probes, an actor so that Actor's PointRegion
// is in scope and Trace and Spawn are its own; each line logged with
// "DXNONE:".
//=============================================================================
class CapNoneProbe extends Actor;

function Say(string S)
{
	Log("DXNONE: " $ S);
}

function Probes()
{
	local Actor A;
	local Pawn P;
	local NavigationPoint NP;
	local Mover M;
	local DeusExLevelInfo L;
	local CapDoomed D;
	local Texture T;
	local int I;
	local float F;
	local byte Bt;
	local bool B;
	local name N;
	local string S;
	local vector V, HL, HN;
	local rotator R;
	local PointRegion Region;
	local Actor Hit;

	// An element written through None, as AutoTurret's gun.MultiSkins[2] = None
	A.MultiSkins[2] = None;
	Say("wrote an element through None");
	// Elements read through None, into variables set before
	T = Texture'Engine.DefaultTexture';
	T = A.MultiSkins[1];
	Say("object element " $ T);
	I = 5;
	I = NP.Paths[2];
	Say("int element " $ I);
	V = vect(1, 2, 3);
	V = M.KeyPos[2];
	Say("vector element " $ V);
	S = "kept";
	S = L.startupMessage[0];
	Say("string element [" $ S $ "]");
	B = (A.MultiSkins[1] != None);
	Say("element compare " $ B $ " " $ (L.startupMessage[0] != "") $ " " $ (NP.Paths[2] == 0) $ " " $ (M.KeyPos[2] == vect(0, 0, 0)));
	// Members of each type through None, into variables set before
	B = True;
	B = A.bHidden;
	Bt = 9;
	Bt = A.Style;
	I = 5;
	I = A.LatentInt;
	F = 1.5;
	F = A.DrawScale;
	N = 'kept';
	N = A.Tag;
	S = "kept";
	S = P.MenuName;
	V = vect(1, 2, 3);
	V = A.Location;
	R = rot(1, 2, 3);
	R = A.Rotation;
	T = Texture'Engine.DefaultTexture';
	T = A.Texture;
	Say("members " $ B $ " " $ Bt $ " " $ A.LightRadius $ " " $ I $ " " $ F $ " " $ N $ " [" $ S $ "] " $ V $ " " $ R $ " " $ T);
	Say("in expressions " $ (A.DrawScale + 2.5) $ " " $ (A.Location + vect(1, 1, 1)) $ " " $ (A.Tag == '') $ " " $ (A.Tag == 'None') $ " " $ (A.LightRadius > 0) $ " " $ (!A.bHidden) $ " " $ (A.Texture == None) $ " " $ (P.MenuName == "") $ " " $ (A.Rotation == rot(0, 0, 0)));
	// A switch on a byte, a name and a string through None: the zero case, or the default
	switch (A.Physics)
	{
		case PHYS_None: Say("switch PHYS_None"); break;
		case PHYS_Walking: Say("switch PHYS_Walking"); break;
		default: Say("switch default"); break;
	}
	switch (A.Tag)
	{
		case 'None': Say("switch name None"); break;
		default: Say("switch name default"); break;
	}
	switch (P.MenuName)
	{
		case "": Say("switch string empty"); break;
		default: Say("switch string default"); break;
	}
	// A struct through None: copied, compared, its members read; nested contexts
	Region.ZoneNumber = 7;
	Region.iLeaf = 8;
	Region = A.Region;
	Say("struct " $ Region.Zone $ " " $ Region.iLeaf $ " " $ Region.ZoneNumber $ " equal " $ (Region == A.Region) $ " " $ (Region != A.Region) $ " member " $ A.Region.ZoneNumber $ " " $ A.Region.iLeaf $ " " $ A.Region.Zone);
	Say("nested " $ A.Owner.Location $ " " $ A.Owner.Region.ZoneNumber $ " " $ A.Base.Tag $ " " $ (A.Owner.Owner == None));
	// A script call and native calls through None
	B = True;
	B = A.IsA('Actor');
	S = "kept";
	S = P.KillMessage('x', None);
	F = 1.5;
	F = A.FRand();
	Say("calls " $ B $ " [" $ S $ "] " $ F $ " [" $ A.GetURLMap() $ "]");
	// A native's optional argument through None: Trace's start, Spawn's location
	Hit = Trace(HL, HN, vect(0, 0, -100000), A.Location);
	Say("trace " $ Hit $ " " $ HL $ " " $ HN);
	Hit = Trace(HL, HN, vect(0, 0, -100000), A.Owner.Location);
	Say("trace nested " $ Hit $ " " $ HL $ " " $ HN);
	D = Spawn(class'CapDoomed',,, A.Location);
	if (D != None)
	{
		Say("spawned at " $ D.Location);
		D.Destroy();
	}
	else
	{
		Say("spawned None");
	}
}

