//=============================================================================
// CapSpot: SwitchConsole's spotlight, a steady white spotlight that can be
// spawned and turned (a moving light: neither bStatic nor bNoDelete), its
// cone 64 of 256, lighting 300 units along its aim.
//=============================================================================
class CapSpot extends Light;

defaultproperties
{
	bStatic=False
	bNoDelete=False
	bMovable=True
	LightType=LT_Steady
	LightEffect=LE_Spotlight
	LightCone=64
	LightBrightness=255
	LightHue=0
	LightSaturation=255
	LightRadius=12
}
