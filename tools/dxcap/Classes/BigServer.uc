//=============================================================================
// BigServer: the serving side of the full bunch test (BigJoinConsole), one
// of a dedicated server's ServerActors (tools/dxcap.sh dedicated <engine>
// BigServer 'DXMP_Cathedral?game=DXCapture.CapDeathMatch' 60). bugs.md
// D.56.
//
// 3 s after a player is in, a CapBigString; then a stage each, logged with
// "DXBIG:": 2 s on, its string "abc"; 5 s on, 600 x's, more than a bunch
// holds (a packet's 512 bytes less 81 bits: FOutBunch, Engine.dll
// 0x103fa3b0), so it never fits; 9 s on, "xyz". The original's
// ReplicateActor marks the bunch after a field's number and its element,
// before the value (Engine.dll 0x103ff3dd), and pops back to that mark when
// the value overflows (0x103ff498): the field's number goes with no value,
// which the client reads past the bunch's end, as an empty string. Exits
// 14 s after placing.
//=============================================================================
class BigServer extends Info;

var float MapTime, InTime, PlacedTime;
var CapBigString Shown;
var int Step;

event Tick(float Delta)
{
	local DeusExPlayer Other, Joined;
	local string S;
	local int i;
	local float T;

	MapTime += Delta;
	foreach AllActors(class'DeusExPlayer', Other)
		Joined = Other;
	if (Joined == None && Shown == None)
		return;
	InTime += Delta;
	if (Shown == None && Step == 0 && InTime > 3.0)
	{
		Shown = Spawn(class'CapBigString',,, Joined.Location + vect(0,0,200));
		PlacedTime = MapTime;
		Log("DXBIG: server placed " $ Shown);
		Step = 1;
		return;
	}
	if (Shown == None)
		return;
	T = MapTime - PlacedTime;
	if (Step == 1 && T >= 2.0)
	{
		Shown.Big = "abc";
		Log("DXBIG: server stage 1: 3 characters");
		Step = 2;
	}
	else if (Step == 2 && T >= 5.0)
	{
		for (i = 0; i < 60; i++)
			S = S $ "xxxxxxxxxx";
		Shown.Big = S;
		Log("DXBIG: server stage 2: " $ Len(Shown.Big) $ " characters");
		Step = 3;
	}
	else if (Step == 3 && T >= 9.0)
	{
		Shown.Big = "xyz";
		Log("DXBIG: server stage 3: 3 characters");
		Step = 4;
	}
	else if (Step == 4 && T >= 14.0)
	{
		Log("DXBIG: server exiting");
		ConsoleCommand("exit");
		Step = 5;
	}
}
