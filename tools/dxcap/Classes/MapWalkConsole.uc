//=============================================================================
// MapWalkConsole: every map of the Maps folder opened in turn, logged with
// "DXMAPWALK:" (docs/DEVELOPMENT.md, scripted runs). From wherever the run
// starts, 1.5 s on each map, then the next by GetMapName, Entry left out;
// each map's name and its count of actors once it is up (the level's clock
// starting over says so, the map's name being the same when a map is opened
// from itself); after the last, an exit. A map the engine refuses -- its
// loader's checks of a model's indexes (bugs.md E10a.3) -- ends the run
// before "done", with the refusal in the log. DXEngine alone: the original's
// loads are not in question.
//=============================================================================
class MapWalkConsole extends Console;

var float MapTime;
var float LastTime;
var bool bOpening;
var string FirstMap;
var string LastOpened;
var int Count;
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
	i = InStr(U, "\\");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "\\");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function int Actors(PlayerPawn P)
{
	local Actor A;
	local int n;

	foreach P.AllActors(class'Actor', A)
		n++;
	return n;
}

function string Bare(string Map)
{
	local int i;

	i = InStr(Caps(Map), ".DX");
	if (i >= 0)
		Map = Left(Map, i);
	return Map;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Next;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	P.ReducedDamageType = 'All';
	if (bOpening)
	{
		if (P.Level.TimeSeconds >= LastTime)
		{
			LastTime = P.Level.TimeSeconds;
			return;
		}
		bOpening = false;
		MapTime = 0;
		Count++;
		Log("DXMAPWALK: " $ Count $ ": " $ MapOf(P) $ ", " $ Actors(P) $ " actors");
	}
	MapTime += Delta;
	if (MapTime < 1.5)
		return;

	Next = P.GetMapName("", LastOpened, 1);
	if (FirstMap == "")
		FirstMap = Next;
	else if (Next == FirstMap || Next == "")
	{
		Log("DXMAPWALK: done, " $ Count $ " maps, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
		return;
	}
	LastOpened = Next;
	if (Bare(Next) ~= "Entry")
		return;
	bOpening = true;
	LastTime = P.Level.TimeSeconds;
	P.ConsoleCommand("open " $ Bare(Next));
}
