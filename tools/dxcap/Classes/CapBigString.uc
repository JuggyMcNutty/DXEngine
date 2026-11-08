//=============================================================================
// CapBigString: the full bunch test's actor (BigServer, BigJoinConsole).
// bugs.md D.56. Always relevant, hidden, a simulated proxy on the
// client; it holds one replicated string, which the server sets a stage at a
// time. On the client, each time the string changes -- its text, not only
// its length: stage 3's "xyz" is as long as stage 1's "abc", so a log keyed
// on the length alone shows nothing of it from a server that never sent the
// 600 characters -- its length and first three characters are logged with
// "DXBIG:" and the time since it arrived.
//=============================================================================
class CapBigString extends Actor;

var string Big;

var string LastBig;
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
	if (!bSeen || Big != LastBig)
	{
		bSeen = true;
		LastBig = Big;
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
}
