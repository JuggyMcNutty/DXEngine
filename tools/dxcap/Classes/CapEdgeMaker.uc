//=============================================================================
// CapEdgeMaker (bugs.md AI.21): EventEdgesConsole's sender, an Info whose
// Ping raises its EventName as a pulse (AISendEvent, audio, 1, 4000), None
// where none is set.
//=============================================================================
class CapEdgeMaker extends Info;

var name EventName;

function Ping()
{
	AISendEvent(EventName, EAITYPE_Audio, 1.0, 4000);
}
