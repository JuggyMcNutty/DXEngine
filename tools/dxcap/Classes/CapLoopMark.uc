//=============================================================================
// CapLoopMark: WalkConsole's marker, an actor that is not hidden and draws
// nothing, so that VisibleActors counts it.
//=============================================================================
class CapLoopMark extends Actor;

defaultproperties
{
	bHidden=False
	DrawType=DT_None
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
