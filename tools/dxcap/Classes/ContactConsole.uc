//=============================================================================
// ContactConsole: the TouchingActors iterator, logged with "DXCONTACT:"
// (docs/DEVELOPMENT.md, scripted runs). From wherever the run starts, 2 s
// in: three CapTouchers spawned 100 units over the player and set onto it
// in turn (SetLocation), each a touch; then the player's TouchingActors of
// class Actor and of class CapToucher listed, the second toucher destroyed,
// and the list again; then an exit. What is compared between the engines:
// every line -- the original walks the Touching slots as the touches filled
// them.
//=============================================================================
class ContactConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXCONTACT: " $ S);
}

function List(PlayerPawn P, string When)
{
	local Actor A;
	local CapToucher T;
	local string S;

	S = When $ ": TouchingActors ";
	foreach P.TouchingActors(class'Actor', A)
		S = S $ A.Name $ " ";
	S = S $ "| CapTouchers ";
	foreach P.TouchingActors(class'CapToucher', T)
		S = S $ T.Name $ " ";
	Say(S);
}

function Probes(PlayerPawn P)
{
	local CapToucher T[3];
	local int i;

	for (i = 0; i < 3; i++)
	{
		T[i] = P.Spawn(class'CapToucher',,, P.Location + vect(0, 0, 100) + vect(30, 0, 0) * i);
		if (T[i] == None)
		{
			Say("no toucher " $ i);
			return;
		}
	}
	for (i = 0; i < 3; i++)
		Say("toucher " $ T[i].Name $ " set onto the player: " $ T[i].SetLocation(P.Location + vect(0, 0, 4) * i));
	List(P, "three set");
	T[1].Destroy();
	List(P, "the second destroyed");
	T[0].Destroy();
	T[2].Destroy();
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes(Viewport.Actor);
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
