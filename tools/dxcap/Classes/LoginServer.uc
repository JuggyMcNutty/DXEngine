//=============================================================================
// LoginServer: the serving side of the login tests (PassJoinConsole,
// ParenJoinConsole), one of a dedicated server's ServerActors
// (tools/dxcap.sh dedicated; the map DXMP_Cathedral?game=DXCapture.
// CapDeathMatch, which lets the scripted client in, with ?GamePassword=dxcap
// for PassJoinConsole). Each player that comes in is logged once, as the
// server has it: its name, its class, its team and the password its login
// gave (GameInfo.Login sets PlayerPawn.Password from the URL's). The
// engine's own lines tell the rest: the login's URL (Login request), a
// refusal (PreLogin failure), the game's GamePassword and Login lines. It
// exits 10 s after the first player came in, so its log is written whole,
// or after 150 s with none. Each line starts "DXLOGINSRV:".
//=============================================================================
class LoginServer extends Info;

var float MapTime, InTime;
var PlayerPawn Seen[8];
var int NumSeen;
var bool bIn, bDone;

event Tick(float Delta)
{
	local PlayerPawn P;
	local int i;
	local bool bKnown;

	MapTime += Delta;
	foreach AllActors(class'PlayerPawn', P)
	{
		bKnown = false;
		for (i = 0; i < NumSeen; i++)
		{
			if (Seen[i] == P)
				bKnown = true;
		}
		if (!bKnown && NumSeen < 8 && P.PlayerReplicationInfo != None)
		{
			Seen[NumSeen] = P;
			NumSeen++;
			Log("DXLOGINSRV: in: '" $ P.PlayerReplicationInfo.PlayerName $ "' class " $ P.Class $ " team " $ P.PlayerReplicationInfo.Team $ " password '" $ P.Password $ "'");
			if (!bIn)
			{
				bIn = true;
				InTime = MapTime;
			}
		}
	}
	if (!bDone && ((bIn && MapTime - InTime > 10.0) || MapTime > 150.0))
	{
		Log("DXLOGINSRV: server exiting");
		ConsoleCommand("exit");
		bDone = true;
	}
}
