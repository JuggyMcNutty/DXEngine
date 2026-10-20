//=============================================================================
// CapSprite: a sprite that can be spawned, with no collision. SpriteConsole
// gives it its texture and style.
//=============================================================================
class CapSprite extends Actor;

defaultproperties
{
	DrawType=DT_Sprite
	Style=STY_Normal
	bHidden=False
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
}
