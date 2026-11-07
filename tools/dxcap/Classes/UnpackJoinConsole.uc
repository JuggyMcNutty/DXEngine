//=============================================================================
// UnpackJoinConsole: the joining side of the unpacking net test
// (UnpackServeConsole; bugs.md NET.27) -- from the menu map it opens the
// server on this machine, and once in the game it waits for the server's
// CapUnpackPawn, here a simulated proxy pawn, which logs on this side with
// "DXUNPACK:" each SimAnim as it arrives and the frame, rate, tween rate
// and last frame the engine unpacked it into, each exactly. The original's
// PostNetReceive makes each the value sent times the double 0.0001,
// 0.0002, 0.001 and 0.0001, as the x87 holds it, stored as a float
// (Engine.dll 0x103683cc-0x1036840b). Exits 2 s after the pawn has seen
// stage 4, or 60 s into the game -- or back in the menu, dropped or never
// in after 40 s.
//=============================================================================
class UnpackJoinConsole extends Console;

var float MenuTime, GameTime, DoneTime;
var int Step;
var CapUnpackPawn Shown;

event Tick(float Delta)
{
	local PlayerPawn P;
	local CapUnpackPawn A;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXUNPACK: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 9 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			// Dropped back to the menu, or never in: exit, so the log is
			// written.
			Log("DXUNPACK: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 9;
		}
		return;
	}

	GameTime += Delta;
	if (Shown == None)
	{
		foreach P.AllActors(class'CapUnpackPawn', A)
			Shown = A;
		if (Shown != None)
			Log("DXUNPACK: client found " $ Shown.Name $ " " $ GameTime $ " s into the game, role " $ Shown.Role);
	}
	if (Step == 1 && Shown != None && Shown.bDone)
	{
		DoneTime = GameTime;
		Step = 2;
	}
	if (Step < 9 && ((Step == 2 && GameTime - DoneTime > 2.0) || GameTime > 60.0))
	{
		Log("DXUNPACK: client exiting " $ GameTime $ " s into the game");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
