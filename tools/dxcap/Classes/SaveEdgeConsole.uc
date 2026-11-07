//=============================================================================
// SaveEdgeConsole: a save's structs and transient objects, written by one
// engine and read by the other. With Write (DXCAP_CONFIG "Write=True")
// Liberty Island is opened as SaveConsole opens it and, 8 s in, the level's
// LevelInfo, which no animation call packs, is given a SimAnim of
// (1, 2, 3, 4) and a SimBlendAnim[2] of (5, 6, 7, 8), two Planes, and an
// EdgeMarker is spawned holding two CapNewProbes made by new on the player,
// Kept flagged RF_Transient (16384) and Plain with no flag; all is logged and
// saved to slot 9 by a SaveHelper, and the run exits 3 s later. Without
// Write, slot 9 is loaded from the menu map as LoadConsole loads it, and 5 s
// into the island the same is logged. The original's save writes a Plane W,
// X, Y, Z, its own member before its Vector's (Core.dll 0x10126610), and no
// object flagged transient (0x1015e5f0): either engine's save loads in
// either with the Planes as they were set, Kept None and Plain its probe.
//=============================================================================
class SaveEdgeConsole extends SaveConsole;

var config bool Write;
var float RunTime;

function string PlaneText(Plane V)
{
	return V.X $ "," $ V.Y $ "," $ V.Z $ "," $ V.W;
}

// The level's two Planes and each marker's objects and class, "DXEDGE:" in
// front: a tag of the native Class, which only a save edited by hand holds,
// is refused at the load, the original logging "Property Class of
// EdgeMarker is not serialiable"
function LogEdges(PlayerPawn P, string Label)
{
	local EdgeMarker M;

	Log("DXEDGE: " $ Label $ " SimAnim " $ PlaneText(P.Level.SimAnim) $ " SimBlendAnim[2] " $ PlaneText(P.Level.SimBlendAnim[2]));
	foreach P.AllActors(class'EdgeMarker', M)
		Log("DXEDGE: " $ Label $ " " $ M.Name $ " Kept " $ string(M.Kept) $ " Plain " $ string(M.Plain) $ " Class " $ string(M.Class));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local SaveHelper S;
	local EdgeMarker M;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;

	if (Write)
	{
		P.ReducedDamageType = 'All';
		if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
		{
			Log("DXEDGE: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = 1;
		}
		else if (Step <= 1 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 8.0)
		{
			P.Level.SimAnim.X = 1;
			P.Level.SimAnim.Y = 2;
			P.Level.SimAnim.Z = 3;
			P.Level.SimAnim.W = 4;
			P.Level.SimBlendAnim[2].X = 5;
			P.Level.SimBlendAnim[2].Y = 6;
			P.Level.SimBlendAnim[2].Z = 7;
			P.Level.SimBlendAnim[2].W = 8;
			M = P.Spawn(class'EdgeMarker');
			M.Kept = new(P, 'EdgeKept', 16384) class'CapNewProbe';
			M.Plain = new(P, 'EdgePlain') class'CapNewProbe';
			LogEdges(P, "saved");
			S = P.Spawn(class'SaveHelper', P);
			S.Slot = 9;
			S.Description = "DXCapture edges";
			Step = 2;
		}
		else if (Step == 2 && MapTime > 11.0)
		{
			Log("DXEDGE: exiting");
			P.ConsoleCommand("exit");
			Step = 3;
		}
		return;
	}

	if (Step == 0 && RunTime > 3.0)
	{
		Log("DXEDGE: loading slot 9");
		DeusExPlayer(P).LoadGame(9);
		Step = 1;
	}
	else if (Step == 1 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 5.0)
	{
		LogEdges(P, "loaded");
		Log("DXEDGE: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
	else if (Step == 1 && RunTime > 60.0)
	{
		Log("DXEDGE: nothing loaded after 60 s, in " $ MapOf(P) $ "; exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
