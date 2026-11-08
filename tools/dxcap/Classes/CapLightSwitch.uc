//=============================================================================
// CapLightSwitch (bugs.md AI.22): LightTreeConsole's light, a
// dynamic actor (not bStatic, so AIGetLightLevel takes it with no line
// check) that draws nothing: white, full brightness, LightRadius 10 (275
// world units). It spawns off (LT_None) unless told otherwise.
//=============================================================================
class CapLightSwitch extends Actor;

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bStatic=False
	bMovable=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	Physics=PHYS_None
	LightType=LT_None
	LightEffect=LE_None
	LightBrightness=255
	LightSaturation=255
	LightRadius=10
}
