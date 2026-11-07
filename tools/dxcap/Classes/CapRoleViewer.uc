//=============================================================================
// CapRoleViewer: RoleGatesConsole's viewers (bugs.md NET.21, NET.22) -- a
// player pawn of the package's own with no player, touching nothing and
// still, that counts its Ticks. Given the remote role autonomous, as a
// server's copy of a client's pawn, the original's tick runs no Tick for it
// and fills its TargetViewRotation, TargetEyeHeight and
// TargetWeaponViewOffset from the pawn it views (Engine.dll 0x103a23ce,
// 0x103a23e6-0x103a249c), whatever the net mode.
//=============================================================================
class CapRoleViewer extends PlayerPawn;

var int Ticks;

function Tick(float Delta)
{
	Ticks++;
}

function string Targets()
{
	return "view rotation " $ TargetViewRotation $ ", eye height " $ TargetEyeHeight $ ", weapon offset " $ TargetWeaponViewOffset;
}

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
