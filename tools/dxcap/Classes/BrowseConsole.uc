//=============================================================================
// BrowseConsole: a browse's edges, logged with "DXBROWSE:" (docs/
// DEVELOPMENT.md, scripted runs). On Liberty Island (opened from the
// original's menu map), 4 s in, the console's switchlevel to UNATCO HQ: the
// game's exec SwitchLevel, whose ServerTravel Deus Ex's LevelInfo passes over
// in standalone. 3 s later a travel to a map that is not there, outside any
// mission (DXCapNoSuchMap), and 3 s after that one numbered within the
// island's (01_DXCapNoSuchMap), where the original's Browse prunes the
// player's augmentations, skills and flags first. 3 s after each it logs the
// map, the player's two progress lines and whether it holds its augmentation
// and skill systems and its flag base. Then the player is given a goal and a
// note, travels to UNATCO HQ as a map exit sends it (ClientTravel,
// TRAVEL_Relative, with the items), and 6 s in logs its goals, notes and
// inventory, restarts the level (RestartLevel: ?restart) and logs them again
// 6 s into the level restarted, then exits.
//=============================================================================
class BrowseConsole extends Console;

var float StepTime;
var int Step;
// A new level by its clock starting again: no reference to the level, which
// would keep the one left from being collected (the original's ?restart
// takes the first DeusExLevelInfo among all objects)
var float LastTime;
var bool bOpened;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

function ReportState(DeusExPlayer P, string When)
{
	Log("DXBROWSE: " $ When $ ": on " $ MapOf(P) $ ", progress '" $ P.ProgressMessage[0] $ "' '"
		$ P.ProgressMessage[1] $ "', augs " $ (P.AugmentationSystem != None) $ " skills "
		$ (P.SkillSystem != None) $ " flags " $ (P.flagBase != None));
}

function ReportRecords(DeusExPlayer P, string When)
{
	local DeusExGoal G;
	local DeusExNote N;
	local Inventory Inv;
	local int Goals, Notes;
	local string Items;

	for (G = P.FirstGoal; G != None; G = G.next)
		Goals++;
	for (N = P.FirstNote; N != None; N = N.next)
		Notes++;
	for (Inv = P.Inventory; Inv != None; Inv = Inv.Inventory)
		Items = Items $ " " $ Inv.Class.Name;
	Log("DXBROWSE: " $ When $ ": on " $ MapOf(P) $ ", goals " $ Goals $ ", notes " $ Notes $ ", inventory:" $ Items);
}

function NextStep()
{
	Step++;
	StepTime = 0;
}

event Tick(float Delta)
{
	local DeusExPlayer P;
	local DeusExGoal Goal;
	local bool bNewLevel;

	Super.Tick(Delta);
	if (Step == 9 || Viewport == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = DeusExPlayer(Viewport.Actor);
	bNewLevel = P.Level.TimeSeconds < LastTime;
	LastTime = P.Level.TimeSeconds;
	if (bNewLevel)
	{
		StepTime = 0;
		bOpened = false;
	}
	StepTime += Delta;

	if (Step == 0)
	{
		if (MapOf(P) != "01_NYC_UNATCOIsland")
		{
			if (!bOpened && StepTime > 1.0)
			{
				bOpened = true;
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			}
		}
		else if (StepTime > 4.0)
		{
			ReportState(P, "start");
			Log("DXBROWSE: switchlevel 01_NYC_UNATCOHQ");
			P.ConsoleCommand("switchlevel 01_NYC_UNATCOHQ");
			NextStep();
		}
	}
	else if (Step == 1 && StepTime > 3.0)
	{
		ReportState(P, "after switchlevel");
		Log("DXBROWSE: travelling to DXCapNoSuchMap");
		P.ClientTravel("DXCapNoSuchMap", TRAVEL_Relative, True);
		NextStep();
	}
	else if (Step == 2 && StepTime > 3.0)
	{
		ReportState(P, "after DXCapNoSuchMap");
		Log("DXBROWSE: travelling to 01_DXCapNoSuchMap");
		P.ClientTravel("01_DXCapNoSuchMap", TRAVEL_Relative, True);
		NextStep();
	}
	else if (Step == 3 && StepTime > 3.0)
	{
		ReportState(P, "after 01_DXCapNoSuchMap");
		Goal = P.AddGoal('DXBrowseGoal', True);
		Goal.SetText("DXCapture goal");
		P.AddNote("DXCapture note", False, False);
		ReportRecords(P, "leaving");
		P.ClientTravel("01_NYC_UNATCOHQ", TRAVEL_Relative, True);
		NextStep();
	}
	else if (Step == 4 && MapOf(P) == "01_NYC_UNATCOHQ" && StepTime > 6.0)
	{
		ReportRecords(P, "arrived");
		Log("DXBROWSE: restarting the level");
		P.RestartLevel();
		NextStep();
	}
	else if (Step == 5 && bNewLevel)
	{
		NextStep();
	}
	else if (Step == 6 && StepTime > 6.0)
	{
		ReportRecords(P, "restarted");
		Log("DXBROWSE: done, exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
