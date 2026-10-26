//=============================================================================
// CapToucher: ContactConsole's actor, which collides with actors, blocks
// nothing and draws nothing, so that set onto the player it touches it.
//=============================================================================
class CapToucher extends Actor;

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bCollideActors=True
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	CollisionRadius=12
	CollisionHeight=12
	Physics=PHYS_None
}
