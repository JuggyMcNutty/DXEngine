//=============================================================================
// ParenJoinConsole: the joining side of a login URL test -- from the menu map
// it opens the server on this machine (a dedicated one, LoginServer among
// its ServerActors) as JC(1), its class DeusEx.MPNSF and a password of
// paren, which a server with no game password takes whole: Core's Parse
// ends the login's URL at a space, CR, LF, tab or comma, so the brackets
// stay in the name and every option after it comes (the server's Login
// request line), where DXEngine's server ended it at the ')' and logged in
// a JC(1 of the default class with no password (bugs.md NET.12). 3 s into
// the game it logs its name and class as the server sent them back, and
// exits -- or back in the menu, or never in after 40 s. Each line starts
// "DXPAREN:".
//=============================================================================
class ParenJoinConsole extends Console;

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
			Log("DXPAREN: opening 127.0.0.1:7790?Name=JC(1)?Class=DeusEx.MPNSF?Password=paren");
			P.ConsoleCommand("open 127.0.0.1:7790?Name=JC(1)?Class=DeusEx.MPNSF?Password=paren");
			Step = 1;
		}
		else if (Step == 1 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXPAREN: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
		return;
	}

	GameTime += Delta;
	if (Step == 1 && GameTime > 3.0 && P.PlayerReplicationInfo != None)
	{
		Log("DXPAREN: in the game as '" $ P.PlayerReplicationInfo.PlayerName $ "', class " $ P.Class $ "; exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
