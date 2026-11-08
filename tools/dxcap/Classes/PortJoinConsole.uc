//=============================================================================
// PortJoinConsole: the joining side of the login URL test on another port --
// from the menu map it opens 127.0.0.1:7791, a dedicated server run with
// DXCAP_PORT=7791 and LoginServer among its ServerActors (bugs.md NET.37).
// The original's pending level logs in with its URL's own text, the host
// left out and the port not the default's kept (UNetPendingLevel::
// NotifyReceivedText, Engine.dll 0x1040b4dd; FURL::String, 0x103f9210):
// ":7791/Index.dx?Name=..." in the server's login request line, which both
// engines' servers read as the map ":7791" and the portal "Index.dx". 3 s
// into the game it logs its name and exits -- or back in the menu, or never
// in after 40 s. Each line starts "DXPORTJOIN:".
//=============================================================================
class PortJoinConsole extends Console;

var float MenuTime, GameTime;
var int Step;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXPORTJOIN: opening 127.0.0.1:7791");
			P.ConsoleCommand("open 127.0.0.1:7791");
			Step = 1;
		}
		else if (Step == 1 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXPORTJOIN: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
		return;
	}

	GameTime += Delta;
	if (Step == 1 && GameTime > 3.0 && P.PlayerReplicationInfo != None)
	{
		Log("DXPORTJOIN: in the game as '" $ P.PlayerReplicationInfo.PlayerName $ "' on " $ P.Level.GetAddressURL() $ "; exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
