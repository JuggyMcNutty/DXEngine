//=============================================================================
// CapSimFall: SimFallServer's actor (bugs.md NET.31). Always relevant, a
// simulated proxy on the client, with bSimFall, which no stock actor sets.
// The original's native list sends a simulated proxy's Physics,
// Acceleration and bBounce only in its first bunch, bSimFall moving none
// of them on (AActor::GetOptimizedRepList, Engine.dll 0x10362f04-
// 0x10362f8e), where the class's statement (bSimFall || ...) sends each
// change. On the client it logs its Physics at each change, from its
// arrival, with "DXSIMFALL:".
//=============================================================================
class CapSimFall extends Actor;

var bool bSeen;
var EPhysics Last;

simulated function Tick(float Delta)
{
	if (Role == ROLE_Authority)
		return;
	if (!bSeen || Physics != Last)
	{
		Log("DXSIMFALL: client physics " $ int(Physics) $ " at " $ Level.TimeSeconds);
		bSeen = true;
		Last = Physics;
	}
}

defaultproperties
{
	DrawType=DT_None
	bHidden=True
	bAlwaysRelevant=True
	RemoteRole=ROLE_SimulatedProxy
	bSimFall=True
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
