//=============================================================================
// ExecParmConsole: an exec function's parameters as
// the console takes them. The original's ScriptConsoleExec (Core.dll
// 0x10150860) imports each parameter with its property's ImportText: a bool
// is 1, True or the localized true in any case, 0, False or the localized
// false, and any other text, or none, is "bad or missing property" with no
// call made (UBoolProperty::ImportText 0x10166380); a class or object is
// found by name among the loaded objects (UObjectProperty::ImportText
// 0x10166e20, UClassProperty 0x10167110). DXEngine's Engine::ExecCommand
// takes a bool as 1 or "true" alone, calls with false where none is given,
// and answers a class parameter "Unsupported value type", the command then
// unknown.
//
// From whatever map the run starts on, Liberty Island opened; 3 s in, with
// cheats on (bCheatsEnabled set as "set DeusEx.JCDentonMale bCheatsEnabled
// True" sets it): KillAll Terrorist, BehindView True, BehindView 1 then a
// bare BehindView, AugAdd AugSpeed; each logged before and after, a tick
// apart. Each line starts "DXPARM:".
//=============================================================================
class ExecParmConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function int Terrorists(PlayerPawn P)
{
	local Actor A;
	local int n;

	foreach P.AllActors(class'Actor', A)
		if (A.IsA('Terrorist'))
			n++;
	return n;
}

function string SpeedAug(PlayerPawn P)
{
	local DeusExPlayer D;
	local Augmentation A;

	D = DeusExPlayer(P);
	if (D == None || D.AugmentationSystem == None)
		return "no augmentation system";
	A = D.AugmentationSystem.FindAugmentation(class'AugSpeed');
	if (A == None)
		return "AugSpeed none";
	return "AugSpeed bHasIt " $ A.bHasIt;
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
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;
	if (MapTime < 3.0)
		return;

	switch (Step)
	{
	case 0:
		P.bCheatsEnabled = true;
		Log("DXPARM: terrorists " $ Terrorists(P) $ "; killall Terrorist");
		P.ConsoleCommand("killall Terrorist");
		Step = 1;
		break;
	case 1:
		Log("DXPARM: terrorists after " $ Terrorists(P));
		Log("DXPARM: bBehindView " $ P.bBehindView $ "; behindview True");
		P.ConsoleCommand("behindview True");
		Step = 2;
		break;
	case 2:
		Log("DXPARM: bBehindView after True " $ P.bBehindView $ "; behindview 1");
		P.ConsoleCommand("behindview 1");
		Step = 3;
		break;
	case 3:
		Log("DXPARM: bBehindView after 1 " $ P.bBehindView $ "; behindview with no argument");
		P.ConsoleCommand("behindview");
		Step = 4;
		break;
	case 4:
		Log("DXPARM: bBehindView after none " $ P.bBehindView);
		P.ConsoleCommand("behindview 0");
		Log("DXPARM: " $ SpeedAug(P) $ "; augadd AugSpeed");
		P.ConsoleCommand("augadd AugSpeed");
		Step = 5;
		break;
	case 5:
		Log("DXPARM: after augadd " $ SpeedAug(P));
		P.bCheatsEnabled = false;
		P.ConsoleCommand("exit");
		Step = 6;
		break;
	}
}
