//=============================================================================
// AnimEndJoinConsole: the joining side of the AnimEnd net test
// (AnimEndServeConsole; bugs.md NET.15) -- from the menu map it opens the
// server on this machine, and once in the game it waits for the server's
// CapAnimPawn, here a simulated proxy pawn, which logs on this side with
// "DXANIMEND:" its roles, each animation as its SimAnim arrives, each
// AnimEnd it is sent -- none in the original, whose AActor::Tick sends a
// simulated proxy pawn none (Engine.dll 0x103a1bdf) --, each stop and each
// loop's wrap, and what each of the server's stages saw. Exits 2 s after
// the pawn has seen the last stage, or 60 s into the game -- or back in
// the menu, dropped or never in after 40 s.
//=============================================================================
class AnimEndJoinConsole extends Console;

var float MenuTime, GameTime, DoneTime;
var int Step;
var CapAnimPawn Shown;

event Tick(float Delta)
{
	local PlayerPawn P;
	local CapAnimPawn A;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXANIMEND: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 9 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			// Dropped back to the menu, or never in: exit, so the log is
			// written.
			Log("DXANIMEND: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
		return;
	}

	GameTime += Delta;
	if (Shown == None)
	{
		foreach P.AllActors(class'CapAnimPawn', A)
			Shown = A;
		if (Shown != None)
			Log("DXANIMEND: client found " $ Shown.Name $ " " $ GameTime $ " s into the game, role " $ Shown.Role);
	}
	if (Step == 1 && Shown != None && Shown.bDone)
	{
		DoneTime = GameTime;
		Step = 2;
	}
	if (Step < 9 && ((Step == 2 && GameTime - DoneTime > 2.0) || GameTime > 60.0))
	{
		Log("DXANIMEND: client exiting " $ GameTime $ " s into the game");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
