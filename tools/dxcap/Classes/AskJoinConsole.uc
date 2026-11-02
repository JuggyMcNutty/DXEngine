//=============================================================================
// AskJoinConsole: the joining side of a net test of calls with a return
// value -- opens the server on this machine (a dedicated one running
// AskServer) and, 2 s after the server's CapAsker reaches it, calls the
// asker's functions, each into a variable set before: an Ask function of each
// type, which goes to the server (int, byte, float, string, name, actor,
// vector; a bool, which the original's LetBool clears; an int into the
// asker's own variable, and one read by a sum, which reads zero); LocalInt,
// which the client's simulated proxy does not run; SimInt, simulated, which
// it runs; and the game's own GivePlayerAugmentation, which goes to the
// server, where no augmentation of the base class is found and nothing
// changes. In the original a call that goes to the other side, or that the
// proxy does not run, writes nothing (Core.dll ProcessInternal 0x1013f690):
// the variable keeps its value. Each line logged with "DXASK:" and told to
// the server (CapAsker.Tell). What the server says (CapAsker.Saw: the calls
// it took, what its own calls to this client left) is logged as it comes,
// and once more 10 s after the asks, before an exit -- or back in the menu,
// or with no asker after 40 s.
//=============================================================================
class AskJoinConsole extends Console;

var float MenuTime, GameTime, SeenTime, AskedTime;
var int Step;
var CapAsker Asker;
var string LastSaw;

function Say(string S)
{
	Log("DXASK: " $ S);
	if (Asker != None)
		Asker.Tell(S);
}

// Each call into a variable set before
function Ask(DeusExPlayer P)
{
	local int I, J;
	local byte B;
	local float F;
	local string S;
	local name N;
	local Actor A;
	local vector V;
	local bool Z;

	Say("the asker: owned by the player " $ (Asker.Owner == P) $ ", role " $ Asker.Role $ ", remote role " $ Asker.RemoteRole);
	I = 7;
	I = Asker.AskInt(5);
	Say("sent int: 7 then AskInt(5) leaves " $ I);
	B = 7;
	B = Asker.AskByte();
	Say("sent byte: 7 then AskByte() leaves " $ B);
	F = 7.5;
	F = Asker.AskFloat();
	Say("sent float: 7.5 then AskFloat() leaves " $ F);
	S = "kept";
	S = Asker.AskString();
	Say("sent string: [kept] then AskString() leaves [" $ S $ "]");
	N = 'Kept';
	N = Asker.AskName();
	Say("sent name: Kept then AskName() leaves " $ N);
	A = P;
	A = Asker.AskActor();
	Say("sent actor: the player then AskActor() leaves the player " $ (A == P) $ ", None " $ (A == None));
	V = vect(1, 2, 3);
	V = Asker.AskVector();
	Say("sent vector: 1,2,3 then AskVector() leaves " $ V);
	Z = True;
	Z = Asker.AskBool();
	Say("sent bool: True then AskBool() leaves " $ Z);
	Asker.Kept = 7;
	Asker.Kept = Asker.AskInt(5);
	Say("sent into the asker's own int: 7 then AskInt(5) leaves " $ Asker.Kept);
	J = Asker.AskInt(5) + 100;
	Say("sent in a sum: AskInt(5) + 100 is " $ J);
	I = 7;
	I = Asker.LocalInt(5);
	Say("not run: 7 then LocalInt(5) leaves " $ I);
	I = 7;
	I = Asker.SimInt(5);
	Say("simulated: 7 then SimInt(5) leaves " $ I);
	if (P.AugmentationSystem != None)
	{
		A = P;
		A = P.AugmentationSystem.GivePlayerAugmentation(class'DeusEx.Augmentation');
		Say("stock: the player then GivePlayerAugmentation(Augmentation) leaves the player " $ (A == P) $ ", None " $ (A == None));
	}
	else
		Say("stock: no augmentation system");
}

event Tick(float Delta)
{
	local DeusExPlayer P;
	local CapAsker Found;

	Super.Tick(Delta);
	if (Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = DeusExPlayer(Viewport.Actor);

	if (P.Level.NetMode != NM_Client)
	{
		MenuTime += Delta;
		if (Step == 0 && MenuTime > 3.0)
		{
			Log("DXASK: opening 127.0.0.1:7790");
			P.ConsoleCommand("open 127.0.0.1:7790");
			Step = 1;
		}
		else if (Step >= 1 && Step < 5 && (GameTime > 0.0 || MenuTime > 40.0))
		{
			Log("DXASK: in the menu, " $ GameTime $ " s of game; exiting");
			P.ConsoleCommand("exit");
			Step = 5;
		}
		return;
	}

	GameTime += Delta;
	if (Asker == None)
	{
		foreach P.AllActors(class'CapAsker', Found)
			Asker = Found;
		if (Asker != None)
		{
			SeenTime = GameTime;
			Log("DXASK: the asker is here");
		}
	}
	if (Asker != None && Asker.Saw != LastSaw)
	{
		LastSaw = Asker.Saw;
		Log("DXASK: the server says " $ LastSaw);
	}

	if (Step == 1 && Asker != None && GameTime - SeenTime > 2.0)
	{
		Ask(P);
		AskedTime = GameTime;
		Step = 2;
	}
	else if (Step == 1 && GameTime > 40.0)
	{
		Log("DXASK: no asker after " $ int(GameTime) $ " s; exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
	else if (Step == 2 && GameTime - AskedTime > 10.0)
	{
		Log("DXASK: at the end the server says " $ LastSaw);
		Log("DXASK: exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
