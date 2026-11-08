//=============================================================================
// CapOpenLoss: the lost-open test's actor (OpenLossServer,
// OpenLossJoinConsole, netdrop.py; bugs.md NET.33). Always relevant, owned
// by the joined player, a simulated proxy on the client. Its opening bunch
// carries Marker, set as it begins play, which tools/dxcap/netdrop.py looks
// for to take that bunch alone out of the server's datagrams for a few
// seconds, its resends with it. On the server, half a second after it is
// placed, it calls ClientPing once, a reliable call to its owner's client,
// so a reliable bunch of its channel reaches the client before the open;
// and it counts each tick, so unreliable updates of its channel come after
// that. The original's client drops each unreliable bunch of a channel
// whose open it has not had (UNetConnection::ReceivedPacket, Engine.dll
// 0x10402fe0: "Received unreliable bunch before open"), holds the reliable
// one, and plays on until the open comes, then has the actor and its call;
// logged on the client with "DXOPENLOSS:".
//=============================================================================
class CapOpenLoss extends Actor;

var string Marker;
var int Count;

var float Age;
var bool bPinged;
var bool bSeen;
var int LastLogged;

replication
{
	reliable if (Role == ROLE_Authority)
		Marker, Count;
	reliable if (Role == ROLE_Authority)
		ClientPing;
}

function PostBeginPlay()
{
	Super.PostBeginPlay();
	Marker = "DXOPENLOSSMARK";
}

simulated function Tick(float Delta)
{
	Age += Delta;
	if (Role == ROLE_Authority)
	{
		Count++;
		if (!bPinged && Age > 0.5)
		{
			bPinged = true;
			Log("DXOPENLOSS: server ping at " $ Age $ " s, count " $ Count);
			ClientPing(1);
		}
		return;
	}
	if (!bSeen)
	{
		bSeen = true;
		LastLogged = Count;
		Log("DXOPENLOSS: client arrived at " $ Level.TimeSeconds $ ", marker '" $ Marker $ "', count " $ Count);
	}
	else if (Count - LastLogged >= 20)
	{
		LastLogged = Count;
		Log("DXOPENLOSS: client count " $ Count);
	}
}

simulated function ClientPing(int N)
{
	Log("DXOPENLOSS: client ping " $ N $ " at " $ Level.TimeSeconds $ ", count " $ Count);
}

defaultproperties
{
	DrawType=DT_None
	bHidden=True
	bAlwaysRelevant=True
	RemoteRole=ROLE_SimulatedProxy
	NetPriority=3.0
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
