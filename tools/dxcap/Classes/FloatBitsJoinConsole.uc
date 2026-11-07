//=============================================================================
// FloatBitsJoinConsole: the joining side of the float bits net test
// (FloatBitsServeConsole; bugs.md NET.17) -- from the menu map it opens the
// server on this machine, and once in the game it logs with "DXFLOATBITS:"
// each change of its own replication info's Deaths as text, which shows a
// -0 apart from a 0 and a NaN as -1.#IND00, each engine's text the
// original's. Exits 2 s after the server's Score of 7, or 60 s into the
// game -- or back in the menu, dropped or never in after 40 s.
//=============================================================================
class FloatBitsJoinConsole extends Console;

var float MenuTime, GameTime, DoneTime;
var int Step, Changes;
var string Seen;

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Now;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXFLOATBITS: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 9 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			// Dropped back to the menu, or never in: exit, so the log is
			// written.
			Log("DXFLOATBITS: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
		return;
	}

	GameTime += Delta;
	if (P.PlayerReplicationInfo != None)
	{
		Now = string(P.PlayerReplicationInfo.Deaths);
		if (Changes == 0 || Now != Seen)
		{
			Changes++;
			Log("DXFLOATBITS: client sees Deaths " $ Now $ " " $ GameTime $ " s into the game, change " $ Changes);
			Seen = Now;
		}
		if (Step == 1 && P.PlayerReplicationInfo.Score > 6.5)
		{
			Log("DXFLOATBITS: client sees the server done, " $ (Changes - 1) $ " changes after the first");
			DoneTime = GameTime;
			Step = 2;
		}
	}
	if (Step < 9 && ((Step == 2 && GameTime - DoneTime > 2.0) || GameTime > 60.0))
	{
		Log("DXFLOATBITS: client exiting " $ GameTime $ " s into the game");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
