//=============================================================================
// LinkConsole: a link's socket closed with its actor -- 2 s into whatever
// map the run starts on, 20 UdpLinks spawned and each bound to its own port
// from 2100 (BindPort), then all destroyed; a second later 20 spawned and
// bound to the same ports again: how many binds succeed in each round,
// logged, then an exit. The original's AInternetLink::Destroy closes the
// socket at once, so the second round binds every port; DXEngine's once
// closed it only when the collector freed the object, the ports held until
// then (bugs.md E8.19). Then a link of the package's own (CapUdpEcho) sends
// A, U+03A9 and e acute to itself on 127.0.0.1 and logs what it receives,
// by its characters' codes. Each line starts "DXLINK:".
//=============================================================================
class LinkConsole extends Console;

var float RunTime;
var int Step;
var UdpLink Links[20];
var CapUdpEcho Echo;

function int BindRound(PlayerPawn P, string Name)
{
	local int i, bound, port;

	for (i = 0; i < 20; i++)
	{
		Links[i] = P.Spawn(class'UdpLink');
		port = Links[i].BindPort(2100 + i, false);
		if (port > 0)
			bound++;
		else
			Log("DXLINK: " $ Name $ ": port " $ (2100 + i) $ " not bound");
	}
	Log("DXLINK: " $ Name $ ": " $ bound $ " of 20 bound");
	return bound;
}

function SendEcho(PlayerPawn P)
{
	Echo = P.Spawn(class'CapUdpEcho');
	Echo.SendSelf("A" $ Chr(937) $ Chr(233));
}

function DestroyAll()
{
	local int i;

	for (i = 0; i < 20; i++)
	{
		if (Links[i] != None)
			Links[i].Destroy();
		Links[i] = None;
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 2.0)
	{
		BindRound(P, "first round");
		DestroyAll();
		Log("DXLINK: all destroyed");
		Step = 1;
	}
	else if (Step == 1 && RunTime > 3.0)
	{
		BindRound(P, "second round");
		DestroyAll();
		Step = 2;
	}
	else if (Step == 2 && RunTime > 4.0)
	{
		SendEcho(P);
		Step = 4;
	}
	else if (Step == 4 && RunTime > 5.0)
	{
		Log("DXLINK: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
