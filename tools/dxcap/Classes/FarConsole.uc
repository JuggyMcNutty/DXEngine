//=============================================================================
// FarConsole: one view held still and shot in both engines, for what lies
// far from it (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, the near
// plane). On TargetMap ([DXCapture.FarConsole], Liberty Island by default)
// the player is taken out of the world -- no collision or physics,
// undetectable, no shadow, the HUD hidden, conversations and datalinks
// stopped as they start -- and its eye held at SpotX, SpotY, SpotZ, or at
// the start's own eye when all three are 0, looking Yaw and Pitch (the
// map's units, 65,536 a turn; the start's own way with Yaw and Pitch both
// 0). After 4 s, Shots shots (2 by default) 2 s apart, each marked for the
// original's grabber and logged with the eye's place, its way and its
// zone; then exit. Each line is logged with "DXFAR:" in front.
//=============================================================================
class FarConsole extends Console;

var config string TargetMap;
var config float SpotX, SpotY, SpotZ;
var config int Yaw, Pitch;
var config int Shots;

var string CurrentMap;
var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Object Stopped;
var vector Eye;
var rotator View;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

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

// A conversation's camera would take the view, and a datalink's text the
// frame's foot.
function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXFAR: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXFAR: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
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

function HoldView(PlayerPawn P)
{
	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.ViewRotation = View;
	P.SetRotation(View);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local string M;
	local int Count;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	M = MapOf(P);
	if (M != CurrentMap)
	{
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None && (M ~= Target()))
		Silence(DXP);

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXFAR: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
		}
		else if (MapTime > 6.0 && DXP != None)
		{
			if (SpotX == 0 && SpotY == 0 && SpotZ == 0)
				Eye = P.Location + vect(0,0,1) * P.BaseEyeHeight;
			else
			{
				Eye.X = SpotX;
				Eye.Y = SpotY;
				Eye.Z = SpotZ;
			}
			View = P.ViewRotation;
			if (Yaw != 0 || Pitch != 0)
			{
				View.Yaw = Yaw;
				View.Pitch = Pitch;
			}
			View.Roll = 0;
			DXP.KillShadow();
			// Seen out in the open, the player would draw the NPCs' fire
			P.bDetectable = false;
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			HoldView(P);
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			Log("DXFAR: the eye at " $ Eye $ " looking " $ View $ " in " $ P.Region.Zone.Name);
			StepTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == -1 && (M ~= Target()))
		Phase = 0;
	else if (Phase == 1)
	{
		HoldView(P);
		Count = Shots;
		if (Count <= 0)
			Count = 2;
		// Settled after 4 s; a shot every 2 s from then, the mark shown for
		// the 0.6 s before each.
		if (StepTime > 4.0 && ShotsTaken < Count)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 3.4;
			}
			else
			{
				Log("DXFAR: shot " $ ShotsTaken $ ", the eye at " $ (P.Location + vect(0,0,1) * P.EyeHeight)
					$ " looking " $ P.ViewRotation $ " in " $ P.Region.Zone.Name);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 2.0;
			}
		}
		else if (ShotsTaken >= Count && StepTime > 3.0)
		{
			Log("DXFAR: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
