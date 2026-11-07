//=============================================================================
// PauseServeConsole: a paused listen server's input (bugs.md NET.25) -- a
// listen server on DXMP_Cathedral (CapDeathMatch: without the stock console
// check), started from the menu map as ServeConsole starts one, for the
// other engine's JoinConsole to join. 3 s after another player is in, the
// level is paused (its Pauser set, as SetPause sets it) for 2 s, and each
// frame of it the other player's pawn -- the server's copy of a remote
// player, its Player its connection -- is given aMouseX 1 for the paused
// tick to read; every 0.5 s, logged with "DXPAUSE:", the aMouseX that
// pawn's PlayerInput took, as its SmoothMouseX over its mouse scale
// (MouseSensitivity x DesiredFOV x 0.01111), and the host's own. Then the
// level unpaused, and an exit 2 s later, or after 140 s.
//
// The original's paused tick reads a player pawn's input through its Player
// (ULevel::Tick, Engine.dll 0x103a57f7), a net connection's reading nothing
// (UNetConnection::ReadInput, 0x1030262b): the remote pawn's PlayerInput
// takes the 1 as given, the host's 0 with no key held.
//=============================================================================
class PauseServeConsole extends Console;

var float CapTime, InTime, PauseTime, LogTime;
var int Phase;
var PlayerPawn Joined;

// The aMouseX a pawn's PlayerInput took, from the SmoothMouseX it made of
// it, its scale worked out as PlayerInput works it
function float TookMouseX(PlayerPawn Q)
{
	local float Scale;

	Scale = Q.MouseSensitivity * (Q.DesiredFOV * 0.01111);
	if (Scale == 0)
		return 0;
	return Q.SmoothMouseX / Scale;
}

event Tick(float Delta)
{
	local PlayerPawn P, Other;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;

	if (Phase == 0 && CapTime > 3.0)
	{
		Log("DXPAUSE: starting the listen server");
		P.ConsoleCommand("start DXMP_Cathedral?game=DXCapture.CapDeathMatch?listen");
		Phase = 1;
	}
	else if (Phase == 1 && P.Level.NetMode != NM_Standalone)
	{
		Log("DXPAUSE: serving " $ P.Level.GetLocalURL() $ ", net mode " $ P.Level.NetMode);
		Phase = 2;
	}
	else if (Phase == 2)
	{
		foreach P.AllActors(class'PlayerPawn', Other)
			if (Other != P)
				Joined = Other;
		if (Joined != None)
			InTime += Delta;
		if (Joined != None && InTime > 3.0)
		{
			P.Level.Pauser = P.PlayerReplicationInfo.PlayerName;
			Log("DXPAUSE: paused, " $ Joined.PlayerReplicationInfo.PlayerName $ "'s pawn given aMouseX 1 each frame; its Player " $ (Joined.Player != None));
			PauseTime = CapTime;
			LogTime = CapTime;
			Joined.aMouseX = 1.0;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		if (Joined == None || Joined.bDeleteMe)
		{
			P.Level.Pauser = "";
			Log("DXPAUSE: server lost the player; exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		if (CapTime - LogTime >= 0.5)
		{
			LogTime = CapTime;
			Log("DXPAUSE: " $ (CapTime - PauseTime) $ " s paused: the joined pawn took aMouseX " $ TookMouseX(Joined) $ ", the host's " $ TookMouseX(P));
		}
		if (CapTime - PauseTime >= 2.0)
		{
			P.Level.Pauser = "";
			Log("DXPAUSE: unpaused");
			Phase = 4;
		}
		else
			Joined.aMouseX = 1.0;
	}
	else if (Phase == 4 && CapTime - PauseTime >= 4.0)
	{
		Log("DXPAUSE: server done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}

	if (Phase >= 1 && Phase < 9 && CapTime > 140.0)
	{
		Log("DXPAUSE: server out of time, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
