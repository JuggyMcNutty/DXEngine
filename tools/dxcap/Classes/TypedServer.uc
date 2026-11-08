//=============================================================================
// TypedServer: a dedicated server's typed command lines (bugs.md D.65),
// which DXEngine reads from its standard input and the original's log window
// takes typed (the SDK's Window/Inc/Window.h, WTerminal::TypeChar). One of
// the server's ServerActors:
//
//   DXCAP_STDIN=tools/dxcap/typedlines.sh tools/dxcap.sh dedicated dxe \
//       TypedServer 'DXMP_Cathedral?game=DXCapture.CapDeathMatch' 90
//
// Logged with "DXTYPED:": the map at each start; a CapHearer spawned 1 s in,
// which logs each message the level broadcasts (a typed SAY's: "heard
// 'DxTypedSay' type Event beep True"); an exit of its own at 80 s, should
// no typed EXIT come. The lines typedlines.sh types, 2 s and more apart,
// from 8 s: SAY DxTypedSay, DxNoSuchWord (Core's "Unrecognized command"),
// GETMAXTICKRATE ("20.000000"), SERVERTRAVEL DXMP_Smuggler (the server's
// "server switch level" and the next map's "DXTYPED: map"), then EXIT
// ("Closing by request", the server gone before this actor's own exit).
// The original, with -log, takes the same lines typed into its log window
// (xdotool type into the window, Return after each); there its outputs go
// to the log file and "Unrecognized command" to the window alone.
//=============================================================================
class TypedServer extends Info;

var float MapTime;
var CapHearer Hearer;

event PostBeginPlay()
{
	Super.PostBeginPlay();
	Log("DXTYPED: map " $ GetURLMap() $ ", net mode " $ Level.NetMode);
}

event Tick(float Delta)
{
	MapTime += Delta;
	if (Hearer == None && MapTime > 1.0)
	{
		Hearer = Spawn(class'CapHearer');
		Log("DXTYPED: hearer " $ Hearer);
	}
	if (MapTime > 80.0)
	{
		Log("DXTYPED: no exit came, exiting");
		ConsoleCommand("exit");
		MapTime = -1000.0;
	}
}

defaultproperties
{
	RemoteRole=ROLE_None
}
