//=============================================================================
// CapViewMarker: the additional views test's marker (ViewsServer,
// ViewsJoinConsole). bugs.md NET.28. A visible actor of the package's
// own -- the Hong Kong market tarp's mesh, as CapUnpackPawn's --, not always
// relevant, touching nothing, owned by no one and silent, so that it is
// relevant to a client only when seen: from the player's view, or from the
// first of its AdditionalViews (dx-reverse-info network.md, replication).
// On a client it logs once when it arrives, with "DXVIEWS:".
//=============================================================================
class CapViewMarker extends Actor;

simulated function PostNetBeginPlay()
{
	Super.PostNetBeginPlay();
	if (Level.NetMode == NM_Client)
		Log("DXVIEWS: client marker arrived, " $ Name $ " at " $ Location);
}

defaultproperties
{
	DrawType=DT_Mesh
	Mesh=LodMesh'DeusExDeco.HKMarketTarp'
	RemoteRole=ROLE_DumbProxy
	bAlwaysRelevant=False
	bHidden=False
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	bProjTarget=False
	Physics=PHYS_None
	CollisionRadius=72.000000
	CollisionHeight=15.000000
}
