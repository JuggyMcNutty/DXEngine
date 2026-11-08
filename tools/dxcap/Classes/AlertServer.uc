//=============================================================================
// AlertServer: one of a dedicated server's ServerActors (tools/dxcap.sh
// dedicated <engine> AlertServer 'DXMP_Cathedral?game=DXCapture.
// CapDeathMatch' 30; the client JoinConsole or PingJoinConsole). bugs.md
// NET.37, NET.34. From the moment a player is in, every half second for 8 s,
// the player's bBadConnectionAlert as the server has it, which no stock
// script reads: the original's connection sets it each stats period, at a
// lag and loss past 0.8 s or a rate the loss leaves under 2,000 bytes a
// second while the player has its channel, or under 2 packets a second
// come in (UNetConnection::Tick, Engine.dll 0x10404e1c-0x10404ef0) -- the
// first period's lag, its sum begun at 9999 (0x10401604), past it. Then
// the server exits. Each line starts "DXALERT:".
//=============================================================================
class AlertServer extends Info;

var float InTime, LogTime;
var bool bDone;

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;

	if (bDone)
		return;
	foreach AllActors(class'DeusExPlayer', Other)
		Joined = Other;
	if (Joined == None)
		return;
	InTime += Delta;
	if (InTime - LogTime >= 0.5)
	{
		LogTime = InTime;
		Log("DXALERT: t=" $ InTime $ " bBadConnectionAlert " $ Joined.bBadConnectionAlert);
	}
	if (InTime > 8.0)
	{
		Log("DXALERT: server exiting");
		ConsoleCommand("exit");
		bDone = true;
	}
}
