//=============================================================================
// PasswordConsole: a local login under a game password -- 2 s into whatever
// map the run starts on, Liberty Island opened with GamePassword=dxcap on
// its URL (GameInfo.InitGame takes it, logging "GamePassword dxcap"); on the
// island, the player logged in is logged with the level's URL, and an exit
// 2 s later. The original's local login runs no PreLogin, so a game password
// stops no single-player start or listen server's host; DXEngine's once ran
// the game's PreLogin with three arguments for its four, the password's
// refusal landing in the fail code it threw on, and the engine exited at the
// login (bugs.md E15b.3). Each line starts "DXPASS:".
//=============================================================================
class PasswordConsole extends Console;

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

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;

	if (Step == 0 && MapTime > 2.0)
	{
		Log("DXPASS: on " $ CurrentMap $ ", opening 01_NYC_UNATCOIsland?GamePassword=dxcap");
		P.ConsoleCommand("open 01_NYC_UNATCOIsland?GamePassword=dxcap");
		Step = 1;
	}
	else if (Step == 1 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 2.0)
	{
		Log("DXPASS: logged in on " $ P.Level.GetLocalURL() $ " as " $ P);
		Step = 2;
	}
	else if (Step == 2 && MapTime > 4.0)
	{
		Log("DXPASS: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
