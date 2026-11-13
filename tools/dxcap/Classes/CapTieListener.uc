//=============================================================================
// CapTieListener (bugs.md AI.23): SenderTieConsole's listener, an Info -- no
// pawn -- that listens for an event by its Listen (AISetEventCallback, its
// callback Heard), with OneScore as its score callback or none: OneScore
// answers 1 for every sender, so all of them tie. Each call kept for the
// console to read: how many, the last state and the last best sender.
//=============================================================================
class CapTieListener extends Info;

var int Calls;
var int LastState;
var Actor LastBest;

function Listen(name EventName, optional name ScoreName)
{
	AISetEventCallback(EventName, 'Heard', ScoreName);
}

function float OneScore(Actor Receiver, Actor Sender, float Score)
{
	return 1.0;
}

function Heard(name EventName, EAIEventState State, XAIParams Params)
{
	Calls++;
	LastState = int(State);
	LastBest = Params.BestActor;
}
