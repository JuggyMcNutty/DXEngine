//=============================================================================
// CapLightItem: an item LightStartConsole gives the player before a map
// exit. On the map it arrives at it logs, with "DXAILIGHT:", the light the
// AI sees by (AIGetLightLevel, the player's own zone's ambient and every
// light reaching the spot) at the player's place and at each of the level's
// first four Light actors' own places, in its TravelPostAccept, which runs
// in the map's load before any tick or frame of it, then at each of its
// first six ticks there (the first before the map's first frame is drawn).
// The original walks the level's actor list at each call (AActor::
// AIGetLightLevel, Engine.dll 0x1036b980), as DXEngine's AILightAt does,
// before the level's first frame too.
//=============================================================================
class CapLightItem extends Inventory;

var bool bArrived;
var int Ticks;

function Probe(string Label)
{
	local Light L;
	local int n;

	if (Pawn(Owner) == None)
	{
		Log("DXAILIGHT: " $ Label $ " no owner");
		return;
	}
	Log("DXAILIGHT: " $ Label $ " " $ Level.TimeSeconds $ " player " $ Owner.AIGetLightLevel(Owner.Location));
	foreach AllActors(class'Light', L)
	{
		Log("DXAILIGHT: " $ Label $ " " $ L.Name $ " " $ Owner.AIGetLightLevel(L.Location));
		n++;
		if (n >= 4)
			break;
	}
}

event TravelPostAccept()
{
	Super.TravelPostAccept();
	bArrived = True;
	Probe("accept");
}

event Tick(float Delta)
{
	if (bArrived && Ticks < 6)
	{
		Ticks++;
		Probe("tick" $ Ticks);
	}
}

defaultproperties
{
	bStasis=False
}
