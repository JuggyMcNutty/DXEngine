//=============================================================================
// MonoSumConsole: the sounds' mix past full scale on one side, and a sound
// behind with UseSurround, recorded in both engines with UseStereo off
// (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, sound). Galaxy's
// plain routine mixes every voice in mono at the mean of its two sides'
// gains, a surround voice's right side added, and saturates the sounds' sum
// only as it adds the music, once (Galaxy.dll 0x10623210, 0x10623eed). On
// Liberty Island at SoundConsole's open spot, every ambient sound hushed and
// the player out of the world, a CapLoopMark is put 400 units away at the
// eye's height, the first of eight ways round the player it can see, and
// turned to the player's right: from it Beep4 at volume 2 (full after
// its fall-off), its radius five times its distance, so its pan is
// seven-eighths right and not pulled to the middle, once, twice, three
// times and four times on the same tick, 2 s apart; then the node turned
// behind the player, one beep and two, 2 s apart (with UseSurround a sound
// behind plays centred, its right side inverted in stereo). Three beeps of
// the player's own come first. Each step is logged with "DXMONO:" and the
// console's clock; tools/dxcap/sound.py --onsets reads each one's peak
// (DXCAP_RECORD=1, DXCAP_INI="Galaxy.GalaxyAudioSubsystem:UseStereo=False
// Galaxy.GalaxyAudioSubsystem:UseSurround=True").
//=============================================================================
class MonoSumConsole extends Console;

var float MapTime;
var float CapTime;
var string CurrentMap;
var int Phase;
var int Step;
var Actor Source;
var float Distance;
var Object Stopped;

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

function Silence(PlayerPawn P)
{
	local DeusExPlayer DXP;

	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		DXP.conPlay.TerminateConversation();
	}
}

function Note(string What)
{
	Log("DXMONO: " $ What $ " at " $ CapTime);
}

// The node to the player's right (Side 0) or behind it (Side 1); the
// listener turns with the view.
function Face(PlayerPawn P, int Side)
{
	local rotator R;

	R = rotator(Source.Location - P.Location);
	R.Pitch = 0;
	R.Roll = 0;
	if (Side == 0)
		R.Yaw -= 16384;
	else
		R.Yaw += 32768;
	P.SetRotation(R);
	P.ViewRotation = R;
}

// Count beeps from the node on one tick, each on a channel of its own
function Stack(int Count)
{
	local int i;

	for (i = 0; i < Count; i++)
		Source.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_None, 2.0, false, 5.0 * Distance, 1.0);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor A;
	local vector Eye, Spot;
	local rotator R;
	local int i;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	CapTime += Delta;
	P.ReducedDamageType = 'All';
	Silence(P);

	if (Phase == 0)
	{
		if (!(CurrentMap ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = -1;
			}
			return;
		}
		if (MapTime < 3.0 || DeusExPlayer(P) == None)
			return;
		foreach P.AllActors(class'Actor', A)
			if (A.AmbientSound != None)
				A.AmbientSound = None;
		P.SetCollision(false, false, false);
		P.bCollideWorld = false;
		P.SetPhysics(PHYS_None);
		P.SetLocation(vect(2864.35, -378.14, 2578.79));
		P.Velocity = vect(0,0,0);
		Eye = P.Location + vect(0,0,1) * P.BaseEyeHeight;
		for (i = 0; i < 8 && Source == None; i++)
		{
			R.Yaw = i * 8192;
			Spot = Eye + 400.0 * vector(R);
			if (P.FastTrace(Spot, Eye))
			{
				Source = P.Spawn(class'CapLoopMark',,, Spot);
				Distance = 400.0;
			}
		}
		if (Source == None)
		{
			Note("no way round in sight, exiting");
			P.ConsoleCommand("exit");
			Phase = 99;
			return;
		}
		Face(P, 0);
		Note("source " $ Source.Name $ ", " $ Distance $ " away, radius " $ 5.0 * Distance $ "; UseStereo "
			$ P.ConsoleCommand("get ini:Engine.Engine.AudioDevice UseStereo") $ ", UseSurround "
			$ P.ConsoleCommand("get ini:Engine.Engine.AudioDevice UseSurround"));
		Phase = 1;
		Step = 0;
		CapTime = 0;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	if (Phase != 1)
		return;
	if (Step < 3 && CapTime > 1.0 + 0.5 * Step)
	{
		P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_None, 1.0, false, 65536.0);
		Note("beep");
		Step++;
	}
	else if (Step >= 3 && Step < 7 && CapTime > 4.0 + 2.0 * (Step - 3))
	{
		Stack(Step - 2);
		Note("right " $ (Step - 2));
		Step++;
	}
	else if (Step == 7 && CapTime > 12.0)
	{
		Face(P, 1);
		Note("turned");
		Step++;
	}
	else if (Step >= 8 && Step < 10 && CapTime > 13.0 + 2.0 * (Step - 8))
	{
		Stack(Step - 7);
		Note("behind " $ (Step - 7));
		Step++;
	}
	else if (Step == 10 && CapTime > 17.0)
	{
		Note("done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}
