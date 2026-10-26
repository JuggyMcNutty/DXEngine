//=============================================================================
// CapLamp: SwitchConsole's lamp, a steady white light that can be spawned
// and switched (a moving light: neither bStatic nor bNoDelete), lighting
// 200 units round it.
//=============================================================================
class CapLamp extends Light;

defaultproperties
{
	bStatic=False
	bNoDelete=False
	bMovable=True
	LightType=LT_Steady
	LightBrightness=255
	LightHue=0
	LightSaturation=255
	LightRadius=8
}
