//=============================================================================
// SpaceURLConsole: a stock travel URL with a space
// before its '#' -- 14_Vandenberg_Sub's map exit DestMap
// "14_OceanLab_Lab.dx #Sunkenlab" (its other, "14_Oceanlab_silo.dx
// #frontgate"; 14_OceanLab_Lab's teleporter URL "14_OceanLab_UC.dx #UC").
// The original's FURL keeps the map "14_OceanLab_Lab.dx " with its space,
// and Windows' file system drops a trailing space, so its load finds the map.
//
// From whatever map the run starts on, 14_Vandenberg_Sub opened; 3 s in its
// player's augmentation and skill systems are logged, then the map exit's
// own travel (GameInfo.SendPlayer: ClientTravel, TRAVEL_Relative, items);
// 5 s on the map it is on, its URL, where the player stands against the
// Sunkenlab teleporter, and the two systems again; then an exit. With Which=1
// the silo's DestMap; Which=2, opened on 14_OceanLab_Lab, the UC
// teleporter's URL; Which=3, opened on 14_Oceanlab_Silo, its map exit to
// Area 51, "15_area51_bunker.dx #bunker_start" (a new mission: DXEngine
// empties Current and loads the silo again); Which=4, opened on
// 14_OceanLab_UC, its teleporter back, "14_OceanLab_Lab.dx #Sunkentunnel";
// Which=5, opened on 14_OceanLab_Lab, the UC teleporter's URL and, 5 s on the
// UC, its teleporter back: the lab, saved into Current as the travel left it
// within the mission, comes back from there, its URL Current's file (the
// original's Browse, DeusEx.dll 0x10010b90, cuts the map's name at its first
// ".dx" for it). Each line starts "DXSPACE:".
//=============================================================================
class SpaceURLConsole extends Console;

var config int Which;
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
	i = InStr(U, "\\");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "\\");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "#");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, " ");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function string StartMap()
{
	if (Which == 2 || Which == 5)
		return "14_OceanLab_Lab";
	if (Which == 3)
		return "14_Oceanlab_Silo";
	if (Which == 4)
		return "14_OceanLab_UC";
	return "14_Vandenberg_Sub";
}

function string TravelURL()
{
	if (Which == 1)
		return "14_Oceanlab_silo.dx #frontgate";
	if (Which == 2 || Which == 5)
		return "14_OceanLab_UC.dx #UC";
	if (Which == 3)
		return "15_area51_bunker.dx #bunker_start";
	if (Which == 4)
		return "14_OceanLab_Lab.dx #Sunkentunnel";
	return "14_OceanLab_Lab.dx #Sunkenlab";
}

function LogSystems(PlayerPawn P, string When)
{
	local DeusExPlayer D;

	D = DeusExPlayer(P);
	if (D == None)
	{
		Log("DXSPACE: " $ When $ ": no DeusExPlayer");
		return;
	}
	Log("DXSPACE: " $ When $ ": AugmentationSystem " $ D.AugmentationSystem $ ", SkillSystem " $ D.SkillSystem);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Teleporter T;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
		Log("DXSPACE: now on " $ CurrentMap);
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= StartMap()))
	{
		if (MapTime > 3.0)
		{
			Log("DXSPACE: opening " $ StartMap());
			P.ConsoleCommand("open " $ StartMap());
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= StartMap()))
		Step = 0;

	if (Step == 0 && MapTime > 3.0)
	{
		LogSystems(P, "before");
		Log("DXSPACE: travelling to [" $ TravelURL() $ "]");
		P.Level.Game.SendPlayer(P, TravelURL());
		Step = 1;
		MapTime = 0;
	}
	else if (Step == 1 && MapTime > 5.0 && Which == 5)
	{
		Log("DXSPACE: on " $ CurrentMap $ ", URL [" $ P.Level.GetLocalURL() $ "]");
		Log("DXSPACE: travelling back to [14_OceanLab_Lab.dx #Sunkentunnel]");
		P.Level.Game.SendPlayer(P, "14_OceanLab_Lab.dx #Sunkentunnel");
		Step = 3;
		MapTime = 0;
	}
	else if ((Step == 1 || Step == 3) && MapTime > 5.0)
	{
		Log("DXSPACE: on " $ CurrentMap $ ", URL [" $ P.Level.GetLocalURL() $ "]");
		LogSystems(P, "after");
		foreach P.AllActors(class'Teleporter', T)
			Log("DXSPACE: teleporter " $ T.Name $ " tag " $ T.Tag $ " at " $ VSize(T.Location - P.Location) $ " from the player");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
