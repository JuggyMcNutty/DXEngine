//=============================================================================
// CapTravelMarker: an actor with a PreTravel function, TravelConsole's
// witness -- logs "DXTRAVEL: PreTravel on <self>" when the engine sends it.
//=============================================================================
class CapTravelMarker extends Info;

function PreTravel()
{
	Log("DXTRAVEL: PreTravel on " $ Self);
}
