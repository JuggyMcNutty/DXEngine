//=============================================================================
// PassJoinConsole: the joining side of a game password test -- from the menu
// map it opens the server on this machine (a dedicated one started with
// ?GamePassword=dxcap, LoginServer among its ServerActors) three times, 8 s
// apart, as PassJoin: with no password, which the game's PreLogin refuses
// (NEEDPW); with a wrong one (WRONGPW); with dxcap, which gets in. Each
// failure code the console's ConnectFailure is given is logged with the URL
// it is to ask again with, and each pair of progress lines the player is
// shown with the seconds they show for: a refusal's is Connection failed
// over the server's FAILURE, Rejected By Server: and the game's message,
// which the FAILCODE after it leaves as it is (bugs.md NET.11). 3 s into
// the game it logs its name and exits -- or at 60 s, never in.
//=============================================================================
class PassJoinConsole extends Console;

var float RunTime, StepTime, GameTime;
var int Step;
var string Line0, Line1;

event ConnectFailure(string FailCode, string URL)
{
	Log("DXPASSJOIN: t=" $ int(RunTime) $ " ConnectFailure '" $ FailCode $ "' '" $ URL $ "'");
}

function OpenServer(PlayerPawn P, string Password)
{
	local string Address;

	Address = "127.0.0.1:7790?Name=PassJoin";
	if (Password != "")
		Address = Address $ "?Password=" $ Password;
	Log("DXPASSJOIN: t=" $ int(RunTime) $ " opening " $ Address);
	P.ConsoleCommand("open " $ Address);
	StepTime = RunTime;
	Step++;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	// Each pair of lines once, as the player is first shown it.
	if (P.ProgressMessage[0] != Line0 || P.ProgressMessage[1] != Line1)
	{
		Line0 = P.ProgressMessage[0];
		Line1 = P.ProgressMessage[1];
		Log("DXPASSJOIN: t=" $ int(RunTime) $ " progress '" $ Line0 $ "' '" $ Line1 $ "' for " $ int(P.ProgressTimeOut - P.Level.TimeSeconds + 0.5) $ " s");
	}

	if (P.Level.NetMode == NM_Client)
	{
		GameTime += Delta;
		if (Step < 9 && GameTime > 3.0 && P.PlayerReplicationInfo != None)
		{
			Log("DXPASSJOIN: in the game as '" $ P.PlayerReplicationInfo.PlayerName $ "'; exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
		return;
	}

	if (Step == 0 && RunTime > 3.0)
		OpenServer(P, "");
	else if (Step == 1 && RunTime - StepTime > 8.0)
		OpenServer(P, "wrong");
	else if (Step == 2 && RunTime - StepTime > 8.0)
		OpenServer(P, "dxcap");
	else if (Step < 9 && RunTime > 60.0)
	{
		Log("DXPASSJOIN: never in the game; exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
