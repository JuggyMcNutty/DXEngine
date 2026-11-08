//=============================================================================
// CapBigString: the full bunch test's actor (BigServer, BigJoinConsole).
// bugs.md D.56. Always relevant, hidden, a simulated proxy on the
// client; it holds one replicated string, which the server sets a stage at a
// time. On the client, each time the string's length changes, its length
// and first three characters are logged with "DXBIG:" and the time since it
// arrived.
//=============================================================================
class CapBigString extends Actor;

var string Big;

var int LastLen;
var bool bSeen;
var float Age;

replication
{
	reliable if (Role == ROLE_Authority)
		Big;
}

simulated function Tick(float Delta)
{
	if (Level.NetMode != NM_Client)
		return;
	Age += Delta;
	if (!bSeen || Len(Big) != LastLen)
	{
		bSeen = true;
		LastLen = Len(Big);
		Log("DXBIG: client at " $ Age $ " s the string is " $ Len(Big) $ " long, '" $ Left(Big, 3) $ "'");
	}
}

defaultproperties
{
	DrawType=DT_None
	bHidden=True
	bAlwaysRelevant=True
	RemoteRole=ROLE_SimulatedProxy
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
	LastLen=-1
}
