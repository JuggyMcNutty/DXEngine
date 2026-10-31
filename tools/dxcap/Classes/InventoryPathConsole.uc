//=============================================================================
// InventoryPathConsole: Pawn.FindBestInventoryPath, which no script of the
// game calls (docs/DEVELOPMENT.md, scripted runs). On Liberty Island, opened
// from wherever the run starts, 3 s in: each pawn of the level asked for its
// best inventory path with a MinWeight of 0 and of 0.0005, predicting
// respawns and not, each answer and the MinWeight after it logged; then an
// exit. Each line starts "DXINVPATH:".
//=============================================================================
class InventoryPathConsole extends Console;

var float MapTime;
var string CurrentMap;
var int Phase;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function Ask(Pawn Who, float Least, bool bPredict)
{
	local float Weight;
	local Actor Route;

	Weight = Least;
	Route = Who.FindBestInventoryPath(Weight, bPredict);
	Log("DXINVPATH: " $ Who.Name $ " at " $ Who.Location $ ", least " $ Least $ ", predicting " $ bPredict $ ": " $ Route $ ", weight " $ Weight $ ", now at " $ Who.Location);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Pawn Who;
	local int Spots;
	local InventorySpot S;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (MapTime < 3.0)
			return;
		foreach P.AllActors(class'InventorySpot', S)
			Spots++;
		Log("DXINVPATH: " $ Spots $ " inventory spots");
		foreach P.AllActors(class'Pawn', Who)
		{
			Ask(Who, 0.0, false);
			Ask(Who, 0.0, true);
			Ask(Who, 0.0005, false);
		}
		Log("DXINVPATH: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
		{
			MapTime = 0;
			Phase = 0;
		}
	}
}
