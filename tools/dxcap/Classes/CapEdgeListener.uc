//=============================================================================
// CapEdgeListener (bugs.md AI.21): EventEdgesConsole's listener, an Info --
// no pawn -- that listens for an event by its Listen (AISetEventCallback,
// its callback Heard), with a score callback or none: NaNScore answers not
// a number (the square root of -1), SlowScore runs BurnLoops steps of a loop
// and answers the score it was given. Each call is counted and, but with
// bQuiet, logged with "DXEVTEDGE:": the listener's level's time, its name,
// the event, the state and the best sender.
//=============================================================================
class CapEdgeListener extends Info;

var int BurnLoops;
var bool bQuiet;
var int Calls;

function Listen(name EventName, optional name ScoreName)
{
	AISetEventCallback(EventName, 'Heard', ScoreName);
}

function float NaNScore(Actor Receiver, Actor Sender, float Score)
{
	local float MinusOne;

	MinusOne = -1.0;
	return Sqrt(MinusOne);
}

function float SlowScore(Actor Receiver, Actor Sender, float Score)
{
	local int i;
	local float X;

	for (i = 0; i < BurnLoops; i++)
		X += i;
	return Score;
}

function Heard(name EventName, EAIEventState State, XAIParams Params)
{
	local string Best;

	Calls++;
	if (bQuiet)
		return;
	Best = "None";
	if (Params.BestActor != None)
		Best = string(Params.BestActor.Name);
	Log("DXEVTEDGE: call " $ Level.TimeSeconds $ " " $ Name $ " event " $ EventName $ " state " $ int(State) $ " best " $ Best);
}
