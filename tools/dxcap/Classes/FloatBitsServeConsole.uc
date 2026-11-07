//=============================================================================
// FloatBitsServeConsole: the serving side of the float bits net test
// (FloatBitsJoinConsole; bugs.md NET.17) -- a listen server on
// DXMP_Cathedral (CapDeathMatch: without the stock console check), started
// from the menu map as ServeConsole starts one. 3 s after another player is
// in, the Deaths of that player's replication info, a float of the
// original's native list for it, set a step every 2 s: 1, 0, -0 (0 negated),
// 0, a NaN (the square root of -1) held 4 s, 0; then its Score set to 7,
// the client's sign to leave. Each step logged with "DXFLOATBITS:" as it is
// made. The original's list compares the floats by their bits (Engine.dll
// 0x1036601c-0x10366028), so each step goes and the held NaN does not go
// again. Exits 8 s after the last step, so the client leaves first, or
// after 140 s.
//=============================================================================
class FloatBitsServeConsole extends Console;

var float CapTime, InTime, StartTime;
var int Phase, Step;
var PlayerPawn Joined;

// Each step once its time from the start has come, in order
function Play(float T)
{
	local float Zero, MinusOne;
	local string What;

	if (Joined == None || Joined.bDeleteMe || Joined.PlayerReplicationInfo == None)
	{
		Log("DXFLOATBITS: server lost the player at step " $ Step $ "; exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 9;
		return;
	}
	Zero = 0.0;
	MinusOne = -1.0;
	switch (Step)
	{
	case 0:
		Joined.PlayerReplicationInfo.Deaths = 1.0;
		What = "Deaths 1";
		break;
	case 1:
		if (T < 2.0)
			return;
		Joined.PlayerReplicationInfo.Deaths = Zero;
		What = "Deaths 0";
		break;
	case 2:
		if (T < 4.0)
			return;
		Joined.PlayerReplicationInfo.Deaths = -Zero;
		What = "Deaths -0";
		break;
	case 3:
		if (T < 6.0)
			return;
		Joined.PlayerReplicationInfo.Deaths = Zero;
		What = "Deaths 0";
		break;
	case 4:
		if (T < 8.0)
			return;
		Joined.PlayerReplicationInfo.Deaths = Sqrt(MinusOne);
		What = "Deaths a NaN, held 4 s";
		break;
	case 5:
		if (T < 12.0)
			return;
		Joined.PlayerReplicationInfo.Deaths = Zero;
		What = "Deaths 0";
		break;
	case 6:
		if (T < 14.0)
			return;
		Joined.PlayerReplicationInfo.Score = 7.0;
		What = "Score 7, done";
		break;
	default:
		return;
	}
	Log("DXFLOATBITS: server step " $ Step $ " " $ T $ " s in: " $ What $ "; Deaths reads " $ Joined.PlayerReplicationInfo.Deaths);
	Step++;
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
		Log("DXFLOATBITS: starting the listen server");
		P.ConsoleCommand("start DXMP_Cathedral?game=DXCapture.CapDeathMatch?listen");
		Phase = 1;
	}
	else if (Phase == 1 && P.Level.NetMode != NM_Standalone)
	{
		Log("DXFLOATBITS: serving " $ P.Level.GetLocalURL() $ ", net mode " $ P.Level.NetMode);
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
			Log("DXFLOATBITS: server sets " $ Joined.PlayerReplicationInfo.PlayerName $ "'s Deaths");
			StartTime = CapTime;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		Play(CapTime - StartTime);
		if (Phase == 3 && Step > 6 && CapTime - StartTime >= 22.0)
		{
			Log("DXFLOATBITS: server done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}

	if (Phase >= 1 && Phase < 9 && CapTime > 140.0)
	{
		Log("DXFLOATBITS: server out of time, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
