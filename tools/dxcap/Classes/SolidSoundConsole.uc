//=============================================================================
// SolidSoundConsole: a sound at the listener's own eyes, the eyes in the open
// and inside solid (bugs.md AUD.18), heard, not seen. Each move is logged
// with "DXSOLIDSOUND:" and the console's clock; the game's audio is recorded
// outside it (DXCAP_RECORD=1) and read with tools/dxcap/sound.py --levels,
// laid by the three beeps. On 01_NYC_UNATCOIsland, 6 s in, the level hushed
// as SoundConsole hushes it, the player taken out of the world where it
// stands and a CapMarker put at its eyes every tick, the line from it to the
// eyes of no length. After SETTLE, three beeps half a second apart; 2 s on,
// the marker plays Ambient.FanLarge, the eyes in the open, for 5 s; then the
// player is put so that its eyes lie in the solid under the floor, the first
// point 32 units under its face or deeper in no leaf of the BSP, for
// 5 s; then in the open again for 5 s; then an exit. Galaxy walks the line
// from the sound's actor to the eyes whatever its length
// (UModel::FastLineCheck, Galaxy.dll 0x10609cb9-0x10609da3), so inside the
// floor the fan falls to a third of its volume over a third of a second
// and rises again in the open.
//=============================================================================
class SolidSoundConsole extends Console;

#exec OBJ LOAD FILE=..\Sounds\Ambient.uax PACKAGE=Ambient

// Seconds between hushing the level and the first beep: sounds already
// playing run out.
const SETTLE = 4.0;

var float CapTime, MapTime, StepTime;
var string CurrentMap;
var int Phase, Step;
var CapMarker Fan;
var vector SpotOpen, SpotSolid;
var Object Stopped;

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

// Nothing sounds but the fan: every other ambient sound silenced, and every
// pawn and everything that watches for the player gone (destroyed, not
// killed, so no mission script hears of a death), as SoundConsole's Hush.
function Hush(PlayerPawn P)
{
	local Actor A;

	foreach P.AllActors(class'Actor', A)
	{
		if (A == P || A == Fan)
			continue;
		if (A.IsA('Pawn') || A.IsA('SecurityCamera') || A.IsA('AutoTurret') || A.IsA('AlarmUnit') || A.IsA('LaserTrigger') || A.IsA('BeamTrigger')
			|| A.IsA('ShellCasing') || A.IsA('Fragment') || A.IsA('Projectile'))
		{
			A.Destroy();
			continue;
		}
		if (A.IsA('Tree') || A.IsA('RandomSounds'))
			A.SetTimer(0, false);
		if (A.AmbientSound != None)
			A.AmbientSound = None;
	}
}

// No datalink or conversation speaks
function Silence(PlayerPawn P)
{
	local DeusExPlayer DXP;

	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		DXP.conPlay.TerminateConversation();
	}
}

function vector Eyes(PlayerPawn P)
{
	return P.Location + vect(0,0,1) * P.EyeHeight;
}

// Where the player stands with its eyes at the first point under Floor, 32
// units under its face and 16 at a time further, that lies in solid, in no
// leaf of the level's BSP (Region.iLeaf -1), found by the marker, which
// collides with nothing: the start's pier has water under its boards.
function vector SolidUnder(PlayerPawn P, vector Floor)
{
	local vector Point;
	local int i;

	for (i = 0; i < 128; i++)
	{
		Point = Floor - vect(0,0,1) * (32 + 16 * i);
		Fan.SetLocation(Point);
		if (Fan.Region.iLeaf == -1)
			break;
	}
	Log("DXSOLIDSOUND: solid " $ (32 + 16 * i) $ " under the floor, at " $ Point $ ", leaf " $ Fan.Region.iLeaf);
	return Point - vect(0,0,1) * P.BaseEyeHeight;
}

function Stand(PlayerPawn P, vector Spot, string Label)
{
	P.SetLocation(Spot);
	P.Velocity = vect(0,0,0);
	Fan.SetLocation(Eyes(P));
	Log("DXSOLIDSOUND: " $ Label $ " at " $ CapTime $ " (eyes " $ Eyes(P) $ ", the marker's leaf " $ Fan.Region.iLeaf $ ", zone " $ Fan.Region.Zone.Name $ ")");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector HitLoc, HitNorm;
	local Actor Hit;
	local string M;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXSOLIDSOUND: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (Fan != None)
		Fan.SetLocation(Eyes(P));

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXSOLIDSOUND: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		if (!(M ~= "01_NYC_UNATCOIsland") || MapTime < 6.0)
			return;
		P.SetCollision(false, false, false);
		P.bCollideWorld = false;
		P.SetPhysics(PHYS_None);
		SpotOpen = P.Location;
		// The floor's face under the player; the player put so that its eyes
		// lie in the solid under it (SolidUnder)
		Hit = P.Trace(HitLoc, HitNorm, P.Location - vect(0,0,1000), P.Location, false);
		Fan = P.Spawn(class'CapMarker',,, Eyes(P));
		if (Hit == None || Fan == None)
		{
			Log("DXSOLIDSOUND: no floor under the player or no marker, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		SpotSolid = SolidUnder(P, HitLoc);
		Fan.SetLocation(Eyes(P));
		Hush(P);
		Log("DXSOLIDSOUND: the floor at " $ HitLoc $ " normal " $ HitNorm);
		Stand(P, SpotOpen, "settling");
		Step = 0;
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2)
	{
		Silence(P);
		if (Step < 3 && StepTime > SETTLE + 0.5 * Step)
		{
			Log("DXSOLIDSOUND: beep at " $ CapTime);
			P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_None, 2.0, false, 4000, 1.0);
			Step++;
		}
		else if (Step == 3 && StepTime > SETTLE + 3.0)
		{
			Fan.SoundVolume = 192;
			Fan.SoundRadius = 64;
			Fan.AmbientSound = Sound'Ambient.Ambient.FanLarge';
			Stand(P, SpotOpen, "open");
			Step++;
		}
		else if (Step == 4 && StepTime > SETTLE + 8.0)
		{
			Stand(P, SpotSolid, "solid");
			Step++;
		}
		else if (Step == 5 && StepTime > SETTLE + 13.0)
		{
			Stand(P, SpotOpen, "open again");
			Step++;
		}
		else if (Step == 6 && StepTime > SETTLE + 18.0)
		{
			Fan.AmbientSound = None;
			Log("DXSOLIDSOUND: done at " $ CapTime $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
