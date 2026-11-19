//=============================================================================
// OwnerCallServer: the serving side (a dedicated server's
// ServerActors entry, as AskServer: dedicated <engine> OwnerCallServer
// DXMP_Cathedral?game=DXCapture.CapDeathMatch; then each engine's
// OwnerCallJoinConsole as the client). 3 s after a player is in, a
// CapOwnerCaller owned by its pawn; from 2 s after that, every 0.5 s for 10
// s, the server calls its ClientOwned(n) and ClientPlain(n). It logs how
// many of each it made and, 5 s later, exits.
// Expected where bNetOwner holds at the call (the original, if its
// ProcessRemoteFunction sets it, or does not clear it after replicating):
// the client logs 20 ClientOwned and 20 ClientPlain "on the client", the
// server none "on the server". DXEngine's server as read: ClientOwned's 20
// "ran on the server", none on the client.
//=============================================================================
class OwnerCallServer extends Info;

var float MapTime, InTime, PlacedTime, LastCall;
var CapOwnerCaller Caller;
var int Made;
var bool bPlaced, bDone;

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;

	MapTime += Delta;
	foreach AllActors(class'DeusExPlayer', Other)
		Joined = Other;
	if (Joined == None)
		return;
	InTime += Delta;
	if (!bPlaced && InTime > 3.0)
	{
		Caller = Spawn(class'CapOwnerCaller', Joined);
		Log("DXNET6: server placed the caller " $ (Caller != None) $ ", owned by the player " $ (Caller != None && Caller.Owner == Joined));
		bPlaced = true;
		PlacedTime = MapTime;
		LastCall = MapTime + 2.0;
	}
	if (Caller == None || bDone)
		return;
	if (Made < 20 && MapTime - LastCall >= 0.5)
	{
		LastCall = MapTime;
		Made++;
		Caller.ClientOwned(Made);
		Caller.ClientPlain(Made);
	}
	if (Made >= 20 && MapTime - LastCall > 5.0)
	{
		Log("DXNET6: server made " $ Made $ " of each; ClientOwned ran here " $ Caller.OwnedHere $ ", ClientPlain ran here " $ Caller.PlainHere $ "; exiting");
		ConsoleCommand("exit");
		bDone = true;
	}
}

defaultproperties
{
	RemoteRole=ROLE_None
}
