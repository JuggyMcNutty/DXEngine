//=============================================================================
// CapMissingRef: MissingRefServer's actor (bugs.md NET.31). Always relevant,
// a dumb proxy shown as a sprite, its Texture one of a package the game's
// package map has not (Moon.utx, which DXMP_Cathedral and its packages do
// not use), loaded on the server. The original's reference to an object the
// map has not goes once, counted as sent (UPackageMapLevel::SerializeObject,
// Engine.dll 0x104089f0); the client has None either way. On the client it
// logs its Texture once when it arrives, with "DXMISSING:".
//=============================================================================
class CapMissingRef extends Actor;

simulated function PostNetBeginPlay()
{
	Super.PostNetBeginPlay();
	if (Level.NetMode == NM_Client)
		Log("DXMISSING: client arrived, " $ Name $ " Texture " $ Texture);
}

defaultproperties
{
	DrawType=DT_Sprite
	bHidden=False
	bAlwaysRelevant=True
	RemoteRole=ROLE_DumbProxy
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
