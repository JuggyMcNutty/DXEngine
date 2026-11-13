//=============================================================================
// AlertServer: one of a dedicated server's ServerActors (tools/dxcap.sh
// dedicated <engine> AlertServer 'DXMP_Cathedral?game=DXCapture.
// CapDeathMatch' 30; the client JoinConsole or PingJoinConsole). bugs.md
// NET.37, NET.34, NET.39. From the moment a player is in, every half second
// for 8 s, and at every change in between, the player's bBadConnectionAlert
// as the server has it, which no stock script reads, and its connection's
// GETPING and GETLOSS: the original's connection sets the alert each stats
// period, at a lag and loss past 0.8 s or a rate the loss leaves under
// 2,000 bytes a second while the player has its channel, or under 2
// packets a second come in (UNetConnection::Tick, Engine.dll
// 0x10404e1c-0x10404ef0), and GETPING answers the period's lag in ms, 9999
// s until the first period ends (0x104015f8); the first period's lag, its
// sum begun at 9999 (0x10401604) and half the machine's clock off its
// first ack (the connection's first frame time, 0x10404bb8), is below 0 on
// a machine up more than twice 9999 s, so a change of GETPING marks each
// period's end. Then the server exits. Each line starts "DXALERT:".
//=============================================================================
class AlertServer extends Info;

var float InTime, LogTime;
var bool bDone, bLogged, bLastAlert;
var string LastPing;

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;
	local string Ping;
	local bool bDue;

	if (bDone)
		return;
	foreach AllActors(class'DeusExPlayer', Other)
		Joined = Other;
	if (Joined == None)
		return;
	InTime += Delta;
	Ping = Joined.ConsoleCommand("GETPING");
	bDue = InTime - LogTime >= 0.5;
	if (bDue)
		LogTime = InTime;
	if (bDue || !bLogged || Ping != LastPing || Joined.bBadConnectionAlert != bLastAlert)
	{
		Log("DXALERT: t=" $ InTime $ " bBadConnectionAlert " $ Joined.bBadConnectionAlert $ " getping '" $ Ping $ "' getloss '" $ Joined.ConsoleCommand("GETLOSS") $ "'");
		bLogged = true;
		LastPing = Ping;
		bLastAlert = Joined.bBadConnectionAlert;
	}
	if (InTime > 8.0)
	{
		Log("DXALERT: server exiting");
		ConsoleCommand("exit");
		bDone = true;
	}
}
