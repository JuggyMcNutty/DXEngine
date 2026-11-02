//=============================================================================
// CapAsker: the net test's asker (AskServer, AskJoinConsole) -- spawned by
// the server for the joined player, owned by its pawn, always relevant, a
// simulated proxy on the client. Its replicated functions each have a return
// value: the Ask ones go from the client to the server, the Client ones from
// the server to the client, and each run on the far side is logged with
// "DXASK:"; the server counts the client's (Took, a bit for each function,
// and Calls). Each returns an answer of its own, so a variable it is
// assigned to shows whether the call ran where it was made (the answer), was
// held off (the variable's own value: the original's ProcessInternal, Core.dll
// 0x1013f690, writes nothing for a call that goes to the other side) or gave
// its type's zero. LocalInt is neither replicated nor simulated, so the
// client's simulated proxy does not run it; SimInt, simulated, runs where it
// is called. Saw is what the server says (AskServer): the calls it took and
// what its own calls left; Tell brings a line of the client's into the
// server's log, the Client calls it took among them. Either side's log so
// holds every line: two originals share the game's folder, and its one log.
//=============================================================================
class CapAsker extends Info;

var int Took, Calls;
var int Kept;
var string Saw;

replication
{
	// What the server says to its client
	reliable if (Role == ROLE_Authority)
		Saw;

	// The client's calls, to the server
	reliable if (Role < ROLE_Authority)
		AskInt, AskByte, AskFloat, AskString, AskName, AskActor, AskVector, AskBool, Tell;

	// The server's calls, to the client
	reliable if (Role == ROLE_Authority)
		ClientInt, ClientString, ClientActor;
}

// A call of the client's, taken on the server
function Taken(string What, int Bit)
{
	Took = Took | Bit;
	Calls++;
	Log("DXASK: server took " $ What);
}

function int AskInt(int N)
{
	Taken("AskInt " $ N, 1);
	return N + 1;
}

function byte AskByte()
{
	Taken("AskByte", 2);
	return 8;
}

function float AskFloat()
{
	Taken("AskFloat", 4);
	return 8.5;
}

function string AskString()
{
	Taken("AskString", 8);
	return "answered";
}

function name AskName()
{
	Taken("AskName", 16);
	return 'Answered';
}

function Actor AskActor()
{
	Taken("AskActor", 32);
	return self;
}

function vector AskVector()
{
	Taken("AskVector", 64);
	return vect(4, 5, 6);
}

function bool AskBool()
{
	Taken("AskBool", 128);
	return true;
}

// A line of the client's, into the server's log
function Tell(string Line)
{
	Log("DXASK: the client says " $ Line);
}

// Neither replicated nor simulated: a simulated proxy does not run it
function int LocalInt(int N)
{
	Log("DXASK: LocalInt ran, net mode " $ Level.NetMode);
	return N + 1;
}

// Simulated: it runs where it is called
simulated function int SimInt(int N)
{
	return N + 1;
}

// A call of the server's, taken on the client: logged, and told to the server
simulated function ClientTook(string What)
{
	Log("DXASK: client took " $ What);
	Tell("client took " $ What);
}

// The server's calls, run on the client
simulated function int ClientInt(int N)
{
	ClientTook("ClientInt " $ N);
	return N + 1;
}

simulated function string ClientString()
{
	ClientTook("ClientString");
	return "answered";
}

simulated function Actor ClientActor()
{
	ClientTook("ClientActor");
	return self;
}

defaultproperties
{
	bHidden=True
	bAlwaysRelevant=True
	RemoteRole=ROLE_SimulatedProxy
}
