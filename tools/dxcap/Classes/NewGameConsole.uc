//=============================================================================
// NewGameConsole: a new game begun from inside another, as the main menu's
// New Game begins one with a weapon drawn. The run's first new game goes to
// Liberty Island (StartNewGame, from wherever the run starts); 6 s in, the
// belt's first slot is drawn (ActivateBelt(1), the 1 key), and 3 s on the
// intro is shown as the menu's New Game shows it (ShowIntro(True)). In
// 00_Intro, 3 s after the player is Paralyzed, Fire skips it as a player's
// Fire does (Paralyzed.Fire, ShowMainMenu, PostIntro: the second new game's
// StartNewGame). On the island again, 6 s in, what the player carries is
// logged -- each item, its state and belt slot -- with what is in hand
// (Weapon, inHand, inHandPending, LastinHand, bInHandTransition), then the
// belt's first slot is drawn and its second after it, each logged 3 s on,
// and the run exits. A first game's weapon that the second's start does not
// clear comes back as a second item of its class.
//=============================================================================
class NewGameConsole extends Console;

var int Step;
var int Game;
var float MapTime;
var float StepTime;
var string CurrentMap;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, "\\");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "\\");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function string StateOf(Actor A)
{
	if (A == None)
		return "None";
	return A.Class.Name $ " " $ A.GetStateName();
}

// What is in hand, logged with "DXNEWGAME:" in front.
function Hand(PlayerPawn P, string Label)
{
	local DeusExPlayer DP;

	DP = DeusExPlayer(P);
	if (DP == None)
		return;
	Log("DXNEWGAME: " $ Label $ " Weapon " $ StateOf(P.Weapon) $ ", inHand " $ StateOf(DP.inHand)
		$ ", inHandPending " $ StateOf(DP.inHandPending) $ ", LastinHand " $ StateOf(DP.LastinHand)
		$ ", bInHandTransition " $ DP.bInHandTransition);
}

// What the player carries, item by item.
function Carried(PlayerPawn P, string Label)
{
	local Inventory Inv;
	local int Count, Pistols;

	for (Inv = P.Inventory; Inv != None; Inv = Inv.Inventory)
	{
		Count++;
		if (Inv.IsA('WeaponPistol'))
			Pistols++;
		Log("DXNEWGAME: " $ Label $ " carries " $ StateOf(Inv) $ " belt " $ Inv.bInObjectBelt $ " " $ Inv.beltPos);
		if (Count > 64)
			break;
	}
	Log("DXNEWGAME: " $ Label $ " carries " $ Count $ " items, " $ Pistols $ " WeaponPistol");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
		Log("DXNEWGAME: in " $ CurrentMap);
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	if (DeusExPlayer(P) == None)
		return;

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 3.0)
	{
		Log("DXNEWGAME: game 1, StartNewGame 01_NYC_UNATCOIsland");
		P.ConsoleCommand("StartNewGame 01_NYC_UNATCOIsland");
		Game = 1;
		Step = 1;
	}
	else if (Step == 1 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 6.0)
	{
		Carried(P, "game 1");
		Hand(P, "game 1");
		Log("DXNEWGAME: game 1, ActivateBelt 1");
		DeusExPlayer(P).ActivateBelt(1);
		StepTime = MapTime;
		Step = 2;
	}
	else if (Step == 2 && MapTime > StepTime + 3.0)
	{
		Hand(P, "game 1 belt 1");
		Log("DXNEWGAME: game 1, ShowIntro True");
		DeusExPlayer(P).ShowIntro(True);
		Step = 3;
	}
	else if (Step == 3 && (CurrentMap ~= "00_Intro"))
	{
		if (P.IsInState('Paralyzed'))
		{
			StepTime = MapTime;
			Step = 4;
		}
	}
	else if (Step == 4 && MapTime > StepTime + 3.0)
	{
		Carried(P, "intro");
		Hand(P, "intro");
		Log("DXNEWGAME: intro, Fire");
		P.Fire(0);
		Game = 2;
		Step = 5;
	}
	else if (Step == 5 && (CurrentMap ~= "01_NYC_UNATCOIsland") && MapTime > 6.0)
	{
		Carried(P, "game 2");
		Hand(P, "game 2");
		Log("DXNEWGAME: game 2, ActivateBelt 1");
		DeusExPlayer(P).ActivateBelt(1);
		StepTime = MapTime;
		Step = 6;
	}
	else if (Step == 6 && MapTime > StepTime + 3.0)
	{
		Hand(P, "game 2 belt 1");
		Log("DXNEWGAME: game 2, ActivateBelt 2");
		DeusExPlayer(P).ActivateBelt(2);
		StepTime = MapTime;
		Step = 7;
	}
	else if (Step == 7 && MapTime > StepTime + 3.0)
	{
		Hand(P, "game 2 belt 2");
		Carried(P, "game 2 at the end");
		Log("DXNEWGAME: exiting");
		P.ConsoleCommand("exit");
		Step = 8;
	}
}
