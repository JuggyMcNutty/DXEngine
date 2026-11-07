//=============================================================================
// ClientOnlyConsole: a map's client-only actor in single player (bugs.md
// ENG.23), logged with "DXCLIENTONLY:". On TargetMap
// ([DXCapture.ClientOnlyConsole]), a map tools/dxcap/clientonly.py wrote
// with ActorName (CrateUnbreakableSmall0) made client-only, in a folder
// DXCAP_MAPS names, 2 s in: the level's net mode, and whether the actor is
// in the level -- its class and place -- then an exit. The original's
// game makes an export that shares a flag with its context, the client's
// and the server's (Core.dll 0x101600d5), so the actor is there in single
// player; ClientOnlyServer asks the same on a dedicated server, where it is
// not.
//=============================================================================
class ClientOnlyConsole extends Console;

var config string TargetMap;
var config string ActorName;

var float MapTime;
var int Step;
var string CurrentMap;

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

function string Named()
{
	if (ActorName == "")
		return "CrateUnbreakableSmall0";
	return ActorName;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor A, Found;

	Super.Tick(Delta);
	if (Step == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	if (TargetMap == "")
	{
		Log("DXCLIENTONLY: no TargetMap, exiting");
		P.ConsoleCommand("exit");
		Step = 9;
		return;
	}
	if (Step == 0)
	{
		// The original starts in its menu map.
		if (!(CurrentMap ~= TargetMap))
		{
			if (MapTime > 3.0)
			{
				Log("DXCLIENTONLY: opening " $ TargetMap);
				P.ConsoleCommand("open " $ TargetMap);
				Step = 1;
			}
			return;
		}
		Step = 1;
	}
	if (Step == 1 && CurrentMap ~= TargetMap && MapTime > 2.0)
	{
		foreach P.AllActors(class'Actor', A)
			if (string(A.Name) ~= Named())
				Found = A;
		Log("DXCLIENTONLY: net mode " $ P.Level.NetMode $ ", " $ P.Level.GetLocalURL());
		if (Found != None)
			Log("DXCLIENTONLY: " $ Named() $ " there: " $ Found.Class $ " at " $ Found.Location);
		else
			Log("DXCLIENTONLY: " $ Named() $ " not there");
		Log("DXCLIENTONLY: done, exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
