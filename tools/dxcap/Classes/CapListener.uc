//=============================================================================
// CapListener: an Info EventRingConsole spawns beside the player, which
// listens for CapPing (AISetEventCallback, its callback Heard) once the
// console, or another listener's call, has it Listen. A listener is no
// pawn, so the event manager hears a sound for it by distance alone (within
// the radius). Each call is logged with "DXEVTRING:": the level's time, the
// listener's name, the state (2, Pulse, for a pulse) and the best sender's
// name. At its first call a listener with a Partner has the partner listen
// too, from within the event manager's calls, and forgets it.
//=============================================================================
class CapListener extends Info;

var CapListener Partner;

function Listen()
{
	AISetEventCallback('CapPing', 'Heard');
}

function Heard(name EventName, EAIEventState State, XAIParams Params)
{
	local string Best;

	Best = "None";
	if (Params.BestActor != None)
		Best = string(Params.BestActor.Name);
	Log("DXEVTRING: call " $ Level.TimeSeconds $ " " $ Name $ " state " $ int(State) $ " best " $ Best);
	if (Partner != None)
	{
		Log("DXEVTRING: " $ Name $ " has " $ Partner.Name $ " listen");
		Partner.Listen();
		Partner = None;
	}
}
