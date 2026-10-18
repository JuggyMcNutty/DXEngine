//=============================================================================
// SitConsole: a sitting NPC on his way to his chair and in it -- Private
// Lloyd (BindName PrivateLloyd) at Liberty Island's UNATCO building, whose
// walk to the chair meets it, so the chair's Bump sends him to his own
// state's CircleToFront (docs/NATIVES.md, changing state). Liberty Island
// is opened from wherever the run starts; 2 s into the level the view is put
// on his chair (his OrderTag's seat), from the clearest of eight lines out
// from it (FastTrace), so he is drawn and his animation runs; the player out
// of the world, unseen, the HUD hidden, conversations stopped as they start.
// Through 40 s of the level's time each change of his state, animation
// sequence and bSitting is logged with the time, and every 2 s his state,
// sequence, frame, physics and place; a shot at 30 s and at 38 s, each
// marked for the original's grabber; then an exit. Each line is logged with
// "DXSIT:" in front.
//=============================================================================
class SitConsole extends Console;

var float MapTime;
var float SampleTime;
var int Phase;
var int ShotsTaken;
var Object Stopped;
var ScriptedPawn Lloyd;
var name LastState, LastSeq;
var bool bLastSitting;
var vector Eye, Look;

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

// A conversation's camera would take the view.
function Silence(DeusExPlayer DXP)
{
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXSIT: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
}

function string Place(vector V)
{
	return int(V.X) $ "," $ int(V.Y) $ "," $ int(V.Z);
}

// The view on Lloyd's chair, the seat his OrderTag names (on Lloyd, with
// none): from 80% along the longest clear line of eight, 300 units at most,
// 40 units up.
function bool Watch(PlayerPawn P)
{
	local ScriptedPawn S;
	local Seat Chair, Found;
	local vector Center, Dir, Best;
	local rotator R;
	local float BestLen, Len;
	local int i;

	foreach P.AllActors(class'ScriptedPawn', S)
		if (S.BindName == "PrivateLloyd")
			Lloyd = S;
	if (Lloyd == None)
	{
		Log("DXSIT: no PrivateLloyd");
		return false;
	}
	foreach P.AllActors(class'Seat', Chair, Lloyd.OrderTag)
		Found = Chair;
	if (Found != None)
		Center = Found.Location;
	else
		Center = Lloyd.Location;
	Center.Z += 40;
	for (i = 0; i < 8; i++)
	{
		R.Yaw = i * 8192;
		Dir = vector(R);
		for (Len = 300; Len >= 60; Len -= 20)
			if (P.FastTrace(Center + Dir * Len, Center))
				break;
		if (Len > BestLen)
		{
			BestLen = Len;
			Best = Dir;
		}
	}
	Eye = Center + Best * BestLen * 0.8;
	Look = Center - vect(0,0,20);
	Log("DXSIT: " $ Lloyd.Name $ " at " $ Place(Lloyd.Location) $ " state " $ Lloyd.GetStateName() $ " orders " $ Lloyd.Orders
		$ " chair " $ Found $ " view at " $ Place(Eye) $ " from " $ int(BestLen * 0.8) $ " units");
	return true;
}

function HoldView(PlayerPawn P)
{
	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.ViewRotation = rotator(Look - Eye);
	P.SetRotation(P.ViewRotation);
}

function Report(float T)
{
	if (Lloyd.GetStateName() != LastState || Lloyd.AnimSequence != LastSeq || Lloyd.bSitting != bLastSitting)
	{
		LastState = Lloyd.GetStateName();
		LastSeq = Lloyd.AnimSequence;
		bLastSitting = Lloyd.bSitting;
		Log("DXSIT: " $ int(T * 10) $ " tenths: state " $ LastState $ " sequence " $ LastSeq $ " sitting " $ bLastSitting);
	}
	if (T >= SampleTime)
	{
		SampleTime += 2.0;
		Log("DXSIT: at " $ int(T) $ " s: state " $ Lloyd.GetStateName() $ " sequence " $ Lloyd.AnimSequence
			$ " frame " $ int(Lloyd.AnimFrame * 10) $ " sitting " $ Lloyd.bSitting $ " physics " $ Lloyd.Physics
			$ " at " $ Place(Lloyd.Location) $ " base " $ Lloyd.Base);
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local float T;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None)
		Silence(DXP);

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXSIT: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = -1;
			}
		}
		else if (P.Level.TimeSeconds > 2.0 && DXP != None)
		{
			if (!Watch(P))
			{
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			DXP.KillShadow();
			P.bDetectable = false;
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			HoldView(P);
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			SampleTime = 2.0;
			Phase = 1;
		}
	}
	else if (Phase == -1 && InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0)
	{
		MapTime = 0;
		Phase = 0;
	}
	else if (Phase == 1)
	{
		HoldView(P);
		T = P.Level.TimeSeconds;
		Report(T);
		// The mark shown for the 0.6 s before each shot.
		if (ShotsTaken < 2 && T > 29.4 + 8 * ShotsTaken)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
			}
			else if (T > 30.0 + 8 * ShotsTaken)
			{
				Log("DXSIT: shot " $ ShotsTaken $ " at " $ int(T) $ " s");
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
			}
		}
		else if (T > 40.0)
		{
			Log("DXSIT: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
