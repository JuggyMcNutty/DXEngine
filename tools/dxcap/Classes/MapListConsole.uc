//=============================================================================
// MapListConsole: the GameDirectory's list of maps, as the Load Map tool
// window reads it (LoadMapWindow, its PopulateMapList), logged with
// "DXMAPLIST:" (bugs.md PKG.36).
//
// The original's GetMapsDirectory (DeusEx.dll 0x10017200) lists the map
// files of each path that names *.dx, in the paths' order, passing over a
// name it has listed already, in any case (appStricmp). DXEngine's lists
// each map folder's files whole (UGameDirectory.cpp PopulateDirectoryList),
// so a map in two folders, or a folder the paths name twice, is listed
// twice.
//
// Run with DXCAP_MAPS=<dir> holding a copy of a stock map named in another
// case, 02_nyc_bar.dx: DXCAP_MAPS's path comes before ..\Maps's, so the
// original lists 02_nyc_bar.dx once and no 02_NYC_Bar.dx; DXEngine before
// the fix lists both. From the menu map, 3 s in: every name the listing
// gives, with its place, each name given more than once, and the count;
// then an exit.
//=============================================================================
class MapListConsole extends Console;

var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	local DeusExPlayer P;
	local GameDirectory Dir;
	local int i, j, Count, Times;
	local string Name;

	Super.Tick(Delta);
	if (bDone || Viewport == None)
		return;
	P = DeusExPlayer(Viewport.Actor);
	if (P == None)
		return;
	RunTime += Delta;
	if (RunTime < 3.0)
		return;
	bDone = true;

	Dir = P.CreateGameDirectoryObject();
	Dir.SetDirType(Dir.EGameDirectoryTypes.GD_Maps);
	Dir.GetGameDirectory();
	Count = Dir.GetDirCount();
	for (i = 0; i < Count; i++)
	{
		Name = Dir.GetDirFilename(i);
		Log("DXMAPLIST: " $ i $ " " $ Name);
		Times = 0;
		for (j = 0; j < i; j++)
		{
			if (Dir.GetDirFilename(j) ~= Name)
				Times++;
		}
		if (Times > 0)
			Log("DXMAPLIST: " $ Name $ " listed again, " $ (Times + 1) $ " times so far");
	}
	Log("DXMAPLIST: " $ Count $ " maps listed");
	CriticalDelete(Dir);
	Log("DXMAPLIST: done, exiting");
	P.ConsoleCommand("exit");
}
