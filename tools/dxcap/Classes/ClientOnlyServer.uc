//=============================================================================
// ClientOnlyServer: a map's client-only actor on a dedicated server (bugs.md
// ENG.23), logged with "DXCLIENTONLY:". One of the server's ServerActors
// (tools/dxcap.sh dedicated <engine> ClientOnlyServer DXCapClientOnly), on a
// map tools/dxcap/clientonly.py wrote with CrateUnbreakableSmall0 made
// client-only, in a folder DXCAP_MAPS names: 2 s into the map, the level's
// net mode and whether the actor is in the level, then an exit. A dedicated
// server's context is the server's alone (-server clears GIsClient), so the
// original's makes no client-only export (Core.dll 0x101600d5): the actor
// is not there, where ClientOnlyConsole finds it in single player.
//=============================================================================
class ClientOnlyServer extends Info;

var float MapTime;
var bool bDone;

event Tick(float Delta)
{
	local Actor A, Found;

	MapTime += Delta;
	if (bDone || MapTime < 2.0)
		return;
	bDone = true;
	foreach AllActors(class'Actor', A)
		if (string(A.Name) ~= "CrateUnbreakableSmall0")
			Found = A;
	Log("DXCLIENTONLY: net mode " $ Level.NetMode $ ", " $ Level.GetLocalURL());
	if (Found != None)
		Log("DXCLIENTONLY: CrateUnbreakableSmall0 there: " $ Found.Class $ " at " $ Found.Location);
	else
		Log("DXCLIENTONLY: CrateUnbreakableSmall0 not there");
	Log("DXCLIENTONLY: done, exiting");
	ConsoleCommand("exit");
}
