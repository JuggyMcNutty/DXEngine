//=============================================================================
// CapOwnerCaller: a probe of bNetOwner at a server's remote call
// (OwnerCallServer, OwnerCallJoinConsole). Spawned by the server for the
// joined player, owned by its pawn, always relevant, a simulated proxy on
// the client. Two server-to-client functions that differ only in their
// statement: ClientOwned's tests bNetOwner, as DeusExPlayer's
// UpdateAugmentationDisplayStatus, AddAugmentationDisplay, ShowHud,
// SetServerTimeDiff ... do; ClientPlain's does not. The client never sends a
// bunch on this actor, so on a DXEngine server its bNetOwner is false at
// every call (NetChannel.cpp ReplicateActor clears it after each update):
// ClientOwned then runs on the server ("ran on the server") instead of going
// to the client. Each run logs a DXNET6: line with where it ran.
//=============================================================================
class CapOwnerCaller extends Info;

var int OwnedHere, PlainHere;

replication
{
	reliable if ((Role == ROLE_Authority) && bNetOwner)
		ClientOwned;
	reliable if (Role == ROLE_Authority)
		ClientPlain;
}

simulated function string Where()
{
	if (Level.NetMode == NM_Client)
		return "on the client";
	return "on the server";
}

simulated function ClientOwned(int N)
{
	OwnedHere++;
	Log("DXNET6: ClientOwned " $ N $ " ran " $ Where() $ " (" $ OwnedHere $ " so far), bNetOwner " $ bNetOwner);
}

simulated function ClientPlain(int N)
{
	PlainHere++;
	Log("DXNET6: ClientPlain " $ N $ " ran " $ Where() $ " (" $ PlainHere $ " so far)");
}

defaultproperties
{
	bAlwaysRelevant=True
	RemoteRole=ROLE_SimulatedProxy
	NetPriority=3.0
}
