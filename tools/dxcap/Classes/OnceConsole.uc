//=============================================================================
// OnceConsole: a sprite animated once (DT_SpriteAnimOnce), the frame of its
// animation its life has reached (docs/DEVELOPMENT.md, scripted runs;
// docs/NATIVES.md, sprites). At Liberty Island's start, level and facing
// the start's way, the HUD hidden, 6 s in: a CapOnceSprite, its class's life
// 8 s, 120 units ahead at eye height, drawn STY_Normal at scale 2 with
// CoreTexMisc.Glass.WhiteNoise_A01, a chain of noise frames each unlike the
// others, three. Its life is then held at each of 7.5, 5.6, 4, 3, 1.5 s and
// 0 (no end) in turn -- at 5.6 and 3 the frame n t rounds to is one past
// the one n t - 1/2 does -- a shot of each 1.4 s on, marked for the
// original's grabber; the chain's frames and the one each life comes to are
// logged; then exit. Each line is logged with "DXONCE:" in front.
//=============================================================================
class OnceConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var CapOnceSprite Sprite;
var texture First;
var int Frames;
var float Lives[6];

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

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

// The chain's frames from its first, to its end or back to the first.
function CountFrames()
{
	local texture T;

	Frames = 0;
	for (T = First; T != None && Frames < 64; T = T.AnimNext)
	{
		Log("DXONCE: frame " $ Frames $ " " $ T);
		Frames++;
		if (T.AnimNext == First)
			break;
	}
	Log("DXONCE: " $ Frames $ " frames");
}

// The frame a life comes to: n t - 1/2 rounded, held to the chain, t the
// share of the class's life gone.
function texture FrameOf(float Life)
{
	local float Gone;
	local int i, n;
	local texture T;

	Gone = FClamp(1.0 - Life / class'CapOnceSprite'.default.LifeSpan, 0.0, 1.0);
	n = Clamp(int(Frames * Gone - 0.5 + 0.5), 0, Frames - 1);
	T = First;
	for (i = 0; i < n; i++)
		T = T.AnimNext;
	return T;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local vector X, Y, Z, Eye;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (Sprite != None && ShotsTaken < 6)
		Sprite.LifeSpan = Lives[ShotsTaken];

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXONCE: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = 1;
			}
		}
		else
		{
			MapTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 6.0 && DXP != None)
		{
			First = Texture(DynamicLoadObject("CoreTexMisc.Glass.WhiteNoise_A01", class'Texture'));
			if (First == None)
			{
				Log("DXONCE: no texture, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			CountFrames();
			R = P.ViewRotation;
			R.Pitch = 0;
			P.ViewRotation = R;
			P.SetRotation(R);
			GetAxes(R, X, Y, Z);
			Eye = P.Location + Z * P.EyeHeight;
			Sprite = P.Spawn(class'CapOnceSprite',,, Eye + X * 120);
			if (Sprite == None)
			{
				Log("DXONCE: no room for the sprite, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Sprite.Texture = First;
			Sprite.Style = STY_Normal;
			Sprite.DrawScale = 2.0;
			Sprite.LifeSpan = Lives[0];
			Log("DXONCE: " $ Sprite.Name $ " at " $ Sprite.Location $ " draw type " $ int(Sprite.DrawType) $ " size "
				$ First.USize $ "x" $ First.VSize $ " class life " $ class'CapOnceSprite'.default.LifeSpan);
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// A shot each 2 s, its life held from the shot before; the mark
		// shown for the 0.6 s before each.
		if (StepTime > 2.0 && ShotsTaken < 6)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 1.4;
			}
			else
			{
				Log("DXONCE: shot " $ ShotsTaken $ " life " $ Sprite.LifeSpan $ ", frame " $ FrameOf(Lives[ShotsTaken]));
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 0.0;
			}
		}
		else if (ShotsTaken >= 6 && StepTime > 2.0)
		{
			Log("DXONCE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}

defaultproperties
{
	Lives(0)=7.5
	Lives(1)=5.6
	Lives(2)=4.0
	Lives(3)=3.0
	Lives(4)=1.5
	Lives(5)=0.0
}
