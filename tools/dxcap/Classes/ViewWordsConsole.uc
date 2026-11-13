//=============================================================================
// ViewWordsConsole: the viewport's and the render's words that change the
// picture, none of which DXEngine takes (bugs.md WIN.28, WIN.29, WIN.31,
// WIN.38), logged with "DXVWORDS:". On Liberty Island (DXEngine's map, the original's
// DXCAP_START), the HUD hidden and the view held still, each step 2 s
// apart, a marked shot after each:
//   shot 1  as the map stands, the player's RendMap and ShowFlags logged;
//   shot 2  `RMODE 6` by the player's ConsoleCommand (UViewport::Exec,
//           Engine.dll 0x10373a60: Actor->RendMap = the number, 1 to 19):
//           the original's RendMap 6, REN_PlainTex, its frame drawn with no
//           light map and every actor unlit (render-dll.md: no light map
//           when RendMap is not REN_DynLight); DXEngine's 5, lit, and its
//           log's "Unknown command: RMODE 6";
//   shot 3  `RMODE 5` again, then DeusExPlayer.Matrix(), the credits'
//           THEREISNOSPOON egg (CreditsWindow.EggFound): its Sprite the
//           Matrix texture and `RMODE 6`: the original's world all Matrix
//           texture at full brightness, DXEngine's the texture lit;
//   shot 4  Matrix() again: Sprite None and `RMODE 5`, alike in both;
//   shot 5  `STAT FPS` (Render.dll URender::Exec 0x10b147b0 toggles its
//           FpsStats, DrawStats 0x10b13a50 then writes "FPS=... Frame=...
//           MSEC Render=... MSEC Nodes=... Polys=... Tex=... TexMem=...K"
//           ten pixels over the view's bottom each frame): the original's
//           line, DXEngine none (its `stat <word>` turns its own stats off);
//           then `STAT FPS` again, off;
//   shot 6  `HIDEACTORS` (ShowFlags & ~8, 0x10373a60's SHOWACTORS and
//           HIDEACTORS): the original's frame without its actors, the
//           ShowFlags logged; then `SHOWACTORS`;
//   then    the level's hidden actors counted, `SHOWALL` (every actor's
//           bHidden cleared), counted again: the original 0, DXEngine as
//           before;
//   shot 7  `STAT NET FPS GLOBAL MESH ZONE OCCLUSION GAME CACHE HARDWARE`,
//           the words in URender::Exec's order (Render.dll 0x10b147b0),
//           every panel DrawStats draws (0x10b13a50): the FPS line over
//           the bottom, and from 16 pixels down at the left GLOBAL's four
//           lines, HARDWARE's -- its line wrapped where its texture pools
//           run past the view's width --, MESH's four, ZONE's,
//           OCCLUSION's four, GAME's and CACHE's, each with a blank line
//           after it, NET's none in single player; the figures each
//           engine's own, their lines and places alike; then the same
//           words again, off;
//   shot 8  `ASTAT AUDIO` and `ASTAT DETAIL` (Galaxy.dll 0x10607070), its
//           PostRender's overlay (0x10608570): "GalaxyAudioSubsystem
//           Statistics" 16 pixels down, then from 24 pixels down, 10 in, a
//           channel 16 pixels apart, its sound by its full name or None,
//           and 8 under it its volume, pitch, radius and priority or "  ...";
//           then both again, off; an exit.
// Run each engine at DXCAP_WINDOW=1024x768, the original with
// DXCAP_RENDERER=D3D; compare shots 2, 3, 5, 6, 7 and 8 between the engines.
//=============================================================================
class ViewWordsConsole extends Console;

var float RunTime;
var int Phase;
var int MarkShot;
var bool bMarking;
var vector Stand;
var rotator Look;

function Hold(PlayerPawn P)
{
	P.SetLocation(Stand);
	P.SetRotation(Look);
	P.ViewRotation = Look;
	P.Velocity = vect(0, 0, 0);
	P.SetPhysics(PHYS_None);
}

function ShowState(PlayerPawn P, string When)
{
	Log("DXVWORDS: " $ When $ ": RendMap " $ P.RendMap $ ", ShowFlags " $ P.ShowFlags $ ", Sprite " $ P.Sprite);
}

function Ask(PlayerPawn P, string W)
{
	Log("DXVWORDS: '" $ W $ "' gave '" $ P.ConsoleCommand(W) $ "'");
}

function int HiddenCount(PlayerPawn P)
{
	local Actor A;
	local int n;

	foreach P.AllActors(class'Actor', A)
		if (A.bHidden)
			n++;
	return n;
}

function Shot(PlayerPawn P, int n, string What)
{
	Log("DXVWORDS: shot " $ n $ ", " $ What);
	Log("DXCAP: shot " $ n);
	P.ConsoleCommand("shot");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	P.ReducedDamageType = 'All';
	if (Phase > 0)
		Hold(P);

	if (Phase == 0 && RunTime > 2.0)
	{
		Stand = P.Location;
		Look = P.ViewRotation;
		Look.Pitch = 0;
		Look.Roll = 0;
		Hold(P);
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).ShowHud(False);
		ShowState(P, "start");
		MarkShot = 1;
		bMarking = true;
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Shot(P, 1, "as the map stands");
		bMarking = false;
		Ask(P, "RMODE 6");
		ShowState(P, "after RMODE 6");
		MarkShot = 2;
		bMarking = true;
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 5.0)
	{
		Shot(P, 2, "RMODE 6");
		bMarking = false;
		Ask(P, "RMODE 5");
		ShowState(P, "after RMODE 5");
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).Matrix();
		ShowState(P, "after Matrix()");
		MarkShot = 3;
		bMarking = true;
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 7.0)
	{
		Shot(P, 3, "the Matrix egg");
		bMarking = false;
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).Matrix();
		ShowState(P, "after Matrix() again");
		MarkShot = 4;
		bMarking = true;
		Phase = 4;
	}
	else if (Phase == 4 && RunTime > 9.0)
	{
		Shot(P, 4, "the egg off");
		bMarking = false;
		Ask(P, "STAT FPS");
		MarkShot = 5;
		bMarking = true;
		Phase = 5;
	}
	else if (Phase == 5 && RunTime > 11.0)
	{
		Shot(P, 5, "STAT FPS");
		bMarking = false;
		Ask(P, "STAT FPS");
		Ask(P, "HIDEACTORS");
		ShowState(P, "after HIDEACTORS");
		MarkShot = 6;
		bMarking = true;
		Phase = 6;
	}
	else if (Phase == 6 && RunTime > 13.0)
	{
		Shot(P, 6, "HIDEACTORS");
		bMarking = false;
		Ask(P, "SHOWACTORS");
		ShowState(P, "after SHOWACTORS");
		Log("DXVWORDS: hidden actors " $ HiddenCount(P));
		Ask(P, "SHOWALL");
		Log("DXVWORDS: hidden actors after SHOWALL " $ HiddenCount(P));
		Phase = 7;
	}
	else if (Phase == 7 && RunTime > 14.0)
	{
		Ask(P, "STAT NET FPS GLOBAL MESH ZONE OCCLUSION GAME CACHE HARDWARE");
		MarkShot = 7;
		bMarking = true;
		Phase = 8;
	}
	else if (Phase == 8 && RunTime > 16.0)
	{
		Shot(P, 7, "the STAT panels");
		bMarking = false;
		Ask(P, "STAT NET FPS GLOBAL MESH ZONE OCCLUSION GAME CACHE HARDWARE");
		Ask(P, "ASTAT AUDIO");
		Ask(P, "ASTAT DETAIL");
		MarkShot = 8;
		bMarking = true;
		Phase = 9;
	}
	else if (Phase == 9 && RunTime > 18.0)
	{
		Shot(P, 8, "ASTAT AUDIO and DETAIL");
		bMarking = false;
		Ask(P, "ASTAT AUDIO");
		Ask(P, "ASTAT DETAIL");
		Phase = 10;
	}
	else if (Phase == 10 && RunTime > 19.0)
	{
		Log("DXVWORDS: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

// The mark for the original's screen grabber, as MatrixConsole's.
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
