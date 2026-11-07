//=============================================================================
// CapRoleTarget: the pawn RoleGatesConsole's viewers view (bugs.md NET.22)
// -- a pawn of the package's own, no player, touching nothing and still,
// whose view rotation, eye height and weapon the console sets.
//=============================================================================
class CapRoleTarget extends Pawn;

defaultproperties
{
	bHidden=True
	bStasis=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	bProjTarget=False
	Physics=PHYS_None
}
