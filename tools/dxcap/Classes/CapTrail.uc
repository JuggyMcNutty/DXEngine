//=============================================================================
// CapTrail: TrailConsole's actor, which trails its owner in PHYS_Trailer;
// SpriteTrail is its sprite kind. Neither collides nor shows.
//=============================================================================
class CapTrail extends Actor;

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bCollideActors=False
	bCollideWorld=False
	Physics=PHYS_Trailer
}
