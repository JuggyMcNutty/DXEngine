//=============================================================================
// CensusNotify: CensusConsole's ear on its level's spawns -- a SpawnNotify
// for every class of actor, each actor handed back as it came and noted by
// the console that spawned it. Transient: a level saved with one in it holds
// none, nor its reference to the console.
//=============================================================================
class CensusNotify extends SpawnNotify
	transient;

var CensusConsole Census;

simulated event Actor SpawnNotification(Actor A)
{
	if (Census != None)
		Census.NoteSpawn(A);
	return A;
}

defaultproperties
{
	ActorClass=Class'Engine.Actor'
	bHidden=True
	RemoteRole=ROLE_None
}
