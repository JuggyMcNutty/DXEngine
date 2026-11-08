//=============================================================================
// OpenLossJoinConsole: the joining side of the lost-open test (bugs.md
// NET.33) -- opens the relay on this machine (tools/dxcap/netdrop.py on
// 7790, in front of a dedicated server running OpenLossServer on 7792). The
// server's CapOpenLoss logs on this side when it arrives and when its call
// comes, with "DXOPENLOSS:". Logs and exits once back in the menu (a
// connection closed), or 24 s into the game.
//=============================================================================
class OpenLossJoinConsole extends Console;

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
			Log("DXOPENLOSS: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 5 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXOPENLOSS: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 5;
		}
		return;
	}

	GameTime += Delta;
	if (Step == 1 && GameTime > 24.0)
	{
		Log("DXOPENLOSS: still in the game at " $ GameTime $ " s; exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
