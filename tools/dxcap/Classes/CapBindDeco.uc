//=============================================================================
// CapBindDeco: EmptyBindConsole's decoration, a DeusExDecoration with no
// BindName and no BarkBindName, out of sight and out of the way: hidden,
// with no collision and no physics. Its PostPostBeginPlay binds its
// conversations, as every DeusExDecoration's does.
//=============================================================================
class CapBindDeco extends DeusExDecoration;

defaultproperties
{
	bHidden=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
