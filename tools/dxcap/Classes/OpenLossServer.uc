//=============================================================================
// OpenLossServer: the serving side of the lost-open test (bugs.md NET.33),
// one of a dedicated server's ServerActors, run behind tools/dxcap/netdrop.py:
//   DXCAP_PORT=7792 tools/dxcap.sh dedicated <engine> OpenLossServer
//     'DXMP_Cathedral?game=DXCapture.CapDeathMatch' 60
//   netdrop.py --listen 7790 --server 127.0.0.1:7792
//     --drop-marker DXOPENLOSSMARK --drop-secs 3 --log drop.log
// 3 s after a player is in, a CapOpenLoss owned by that player, logged with
// "DXOPENLOSS:"; exits 20 s after placing it. The relay takes the actor's
// opening bunch alone out of the datagrams for 3 s, and the client's acks
// of those datagrams, so that its reliable ClientPing and its unreliable
// updates reach the client first and the open comes once the 3 s are over.
//=============================================================================
class OpenLossServer extends Info;

var float InTime, PlacedAt;
var CapOpenLoss Placed;
var int Step;

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;

	foreach AllActors(class'DeusExPlayer', Other)
		Joined = Other;
	if (Joined == None && Placed == None)
		return;
	InTime += Delta;
	if (Step == 0 && InTime > 3.0)
	{
		Placed = Spawn(class'CapOpenLoss', Joined,, Joined.Location + vect(0,0,200));
		PlacedAt = Level.TimeSeconds;
		Log("DXOPENLOSS: server placed " $ Placed $ " for " $ Joined);
		Step = 1;
	}
	else if (Step == 1 && Level.TimeSeconds - PlacedAt > 20.0)
	{
		Log("DXOPENLOSS: server exiting");
		ConsoleCommand("exit");
		Step = 2;
	}
}
