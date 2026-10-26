//=============================================================================
// CapSpinner: SpinConsole's actor, which turns by its RotationRate in
// PHYS_Rotating and logs with "DXSPIN:" its EndedRotation.
//=============================================================================
class CapSpinner extends Actor;

event EndedRotation()
{
	Log("DXSPIN: " $ Name $ " EndedRotation at " $ Rotation);
}

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bCollideActors=False
	bCollideWorld=False
	Physics=PHYS_Rotating
}
