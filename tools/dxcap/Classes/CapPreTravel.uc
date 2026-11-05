//=============================================================================
// CapPreTravel: an actor that logs the PreTravel the game engine sends every
// actor of the level as a travel is set (OpenLineConsole).
//=============================================================================
class CapPreTravel extends Info;

function PreTravel()
{
	Log("DXOPEN: PreTravel at " $ Level.Game.Class.Name);
}
