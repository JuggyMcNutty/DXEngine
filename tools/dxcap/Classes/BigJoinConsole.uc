//=============================================================================
// BigJoinConsole: the joining side of the full bunch test -- opens the
// server on this machine (a dedicated one running BigServer); the server's
// CapBigString logs on this side each change of its string's length, with
// "DXBIG:". Exits 24 s into the game, or back in the menu. bugs.md D.56
// (NET).
//=============================================================================
class BigJoinConsole extends Console;

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
			Log("DXBIG: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 5 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXBIG: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 5;
		}
		return;
	}

	GameTime += Delta;
	if (Step == 1 && GameTime > 24.0)
	{
		Log("DXBIG: exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
