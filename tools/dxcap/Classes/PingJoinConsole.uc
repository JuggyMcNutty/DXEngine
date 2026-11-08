//=============================================================================
// PingJoinConsole: JoinConsole's client (a client of 127.0.0.1:7790, walking,
// out at 25 s), which 8 s and 18 s into the net game logs what its own
// player's ConsoleCommand gives for GETPING and GETLOSS and the ping and loss
// the server sent in its replication info, with "DXPINGJOIN:" (bugs.md
// WIN.20). The original's local player's commands go to its
// viewport (APlayerPawn::execConsoleCommand, Engine.dll 0x103ba200), whose
// GETPING answers "0" whatever the connection (UViewport::Exec, 0x10373aee)
// and which has no GETLOSS (nothing takes it: ""); a server's own count of
// the ping is its connection's (UNetConnection::Exec, 0x10401c50), which
// PlayerReplicationInfo.Timer reads there every 2 s for each remote player.
// DXEngine 6d100090: GETPING the client's best lag in ms, GETLOSS
// its loss, and from a DXEngine server a replication info's ping of 0.
// Run beside WordsServer (or any dedicated server of either engine):
//   tools/dxcap.sh dxe PingJoinConsole DX.dx    (or original PingJoinConsole)
//=============================================================================
class PingJoinConsole extends JoinConsole;

var int Asked;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (P.Level.NetMode != NM_Client || P.PlayerReplicationInfo == None)
		return;
	if ((Asked == 0 && GameTime > 8.0) || (Asked == 1 && GameTime > 18.0))
	{
		Log("DXPINGJOIN: t=" $ int(GameTime) $ " getping '" $ P.ConsoleCommand("GETPING") $ "' getloss '" $ P.ConsoleCommand("GETLOSS") $ "' replicated ping " $ P.PlayerReplicationInfo.Ping $ " loss " $ P.PlayerReplicationInfo.PacketLoss);
		Asked++;
	}
}
