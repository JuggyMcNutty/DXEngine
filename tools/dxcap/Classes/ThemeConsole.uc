//=============================================================================
// ThemeConsole: the colour themes through a new game's travels, as the
// menus make them -- ShowIntro(True) from the menu map to 00_Intro (the
// difficulty screen's), then the intro's end, PostIntro (StartNewGame:
// ResetPlayer, SendPlayer) to Liberty Island. 3 s into each map it logs the
// player's theme manager, its current HUD and menu themes by name, the HUD
// theme's background, border, header and text colours, and the manager's
// list. On the island it then shows the HUD's info link window with a line
// of text, logs the colours it took (HUDBaseWindow's and the info link's
// own), hides the HUD's hit display, which would cover the grabber's mark,
// and shoots it; exits 3 s later. With Direct (DXCAP_CONFIG "Direct=True")
// the island is opened from wherever the run starts and the travels are
// left out: the original under Proton stops at ShowIntro's travel here, its
// log cut short. Each line is logged with "DXTHEME:".
//=============================================================================
class ThemeConsole extends Console;

var config bool Direct;

var float MapTime;
var float StepTime;
var int Phase;
var string CurrentMap;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

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

function string Col(color C)
{
	return C.R $ "," $ C.G $ "," $ C.B;
}

function LogThemes(string Label, DeusExPlayer P)
{
	local ColorThemeManager M;
	local ColorTheme T;
	local string S;
	local int n;

	M = P.ThemeManager;
	if (M == None)
	{
		Log("DXTHEME: " $ Label $ " no theme manager");
		return;
	}
	Log("DXTHEME: " $ Label $ " manager " $ M.Name $ " owner " $ M.Owner $ " HUDThemeName " $ P.HUDThemeName $ " MenuThemeName " $ P.MenuThemeName);
	T = M.GetCurrentHUDColorTheme();
	if (T != None)
		Log("DXTHEME: " $ Label $ " HUD theme " $ T.Name $ " '" $ T.GetThemeName() $ "' background " $ Col(T.GetColorFromName('HUDColor_Background'))
			$ " borders " $ Col(T.GetColorFromName('HUDColor_Borders')) $ " header " $ Col(T.GetColorFromName('HUDColor_HeaderText'))
			$ " text " $ Col(T.GetColorFromName('HUDColor_NormalText')));
	else
		Log("DXTHEME: " $ Label $ " no HUD theme");
	T = M.GetCurrentMenuColorTheme();
	if (T != None)
		Log("DXTHEME: " $ Label $ " menu theme " $ T.Name $ " '" $ T.GetThemeName() $ "'");
	else
		Log("DXTHEME: " $ Label $ " no menu theme");
	// Eight to a line: the original's log takes no line much over 1000
	// characters.
	S = "";
	n = 0;
	for (T = M.FirstColorTheme; T != None && n < 100; T = T.next)
	{
		S = S $ " " $ T.Name;
		n++;
		if (n % 8 == 0)
		{
			Log("DXTHEME: " $ Label $ " list" $ S);
			S = "";
		}
	}
	if (S != "")
		Log("DXTHEME: " $ Label $ " list" $ S);
	Log("DXTHEME: " $ Label $ " list of " $ n);
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow Root;
	local HUDInfoLinkDisplay Link;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
		Log("DXTHEME: map " $ CurrentMap);
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP == None)
		return;

	if (Direct && Phase == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			LogThemes("menu", DXP);
			Log("DXTHEME: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Phase = 2;
		}
		return;
	}
	if (Direct && Phase == 0)
		Phase = 2;

	if (Phase == 0 && MapTime > 3.0)
	{
		LogThemes("menu", DXP);
		Log("DXTHEME: ShowIntro(True)");
		DXP.ShowIntro(True);
		Phase = 1;
	}
	else if (Phase == 1 && CurrentMap ~= "00_Intro" && MapTime > 3.0)
	{
		LogThemes("intro", DXP);
		Log("DXTHEME: PostIntro, start map " $ DXP.strStartMap);
		DXP.PostIntro();
		Phase = 2;
	}
	else if (Phase == 2 && CurrentMap ~= "01_NYC_UNATCOIsland" && MapTime > 3.0)
	{
		LogThemes("island", DXP);
		Root = DeusExRootWindow(DXP.rootWindow);
		if (Root == None || Root.hud == None)
		{
			Log("DXTHEME: no HUD");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Root.ShowHud(True);
		Link = Root.hud.infolink;
		if (Link == None)
			Link = Root.hud.CreateInfoLinkWindow();
		Link.Show();
		Link.DisplayText("ThemeConsole: the info link's text in the HUD theme's colours.");
		Log("DXTHEME: info link " $ Link.Name $ " name " $ Col(Link.colName) $ " text " $ Col(Link.colText) $ " header " $ Col(Link.colHeaderText)
			$ " line " $ Col(Link.colLine) $ " queued " $ Col(Link.colQueued) $ " background " $ Col(Link.colBackground) $ " border " $ Col(Link.colBorder));
		if (Root.hud.hit != None)
			Root.hud.hit.Hide();
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3)
	{
		if (StepTime > 1.0 && !bMarking)
		{
			MarkShot = 0;
			bMarking = true;
		}
		else if (StepTime > 1.6 && bMarking)
		{
			P.ConsoleCommand("shot");
			bMarking = false;
			StepTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4 && StepTime > 3.0)
	{
		Log("DXTHEME: exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
