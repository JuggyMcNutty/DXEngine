//=============================================================================
// AmbientConsole: an ambient sound's volume under the audio subsystem's
// AmbientFactor, recorded in both engines (docs/DEVELOPMENT.md, scripted
// runs; docs/NATIVES.md, sound). On Liberty Island, every ambient sound
// hushed and the player out of the world, a FanLarge 64 units ahead, its
// SoundVolume Volume (5 by default), its radius 4,000: 4 s of it as the ini
// has AmbientFactor, then `set ini:Engine.Engine.AudioDevice AmbientFactor
// <Factor>` (20 by default, past the 0-10 Galaxy's SET holds it to) and 4 s,
// then AmbientFactor 0.7 again and 4 s; three beeps a second before each
// part, the beeps' own channel otherwise quiet. Each step is logged with
// "DXAMBIENT:" and the console's clock, by which a recording is read
// (tools/dxcap/sound.py --levels).
//=============================================================================
class AmbientConsole extends Console;

#exec OBJ LOAD FILE=..\Sounds\Ambient.uax PACKAGE=Ambient

var config float Volume;
var config float Factor;

var float MapTime;
var float CapTime;
var string CurrentMap;
var int Phase;
var Actor Fan;
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

function Beeps(PlayerPawn P)
{
	P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_Misc, 1.0, false, 65536.0);
}

function Note(string What)
{
	Log("DXAMBIENT: " $ What $ " at " $ CapTime);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor A;

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
		if (Volume == 0)
			Volume = 5;
		if (Factor == 0)
			Factor = 20;
		foreach P.AllActors(class'Actor', A)
			if (A.AmbientSound != None)
				A.AmbientSound = None;
		P.SetCollision(false, false, false);
		P.bCollideWorld = false;
		P.SetPhysics(PHYS_None);
		Fan = P.Spawn(class'CapSpot',,, P.Location + vector(P.Rotation) * 64);
		Fan.SoundVolume = Volume;
		Fan.SoundRadius = 160;
		Fan.AmbientSound = Sound'Ambient.Ambient.FanLarge';
		Note("the fan at volume " $ Fan.SoundVolume $ ", the factor " $ P.ConsoleCommand("get ini:Engine.Engine.AudioDevice AmbientFactor"));
		Phase = 1;
		CapTime = 0;
	}
	else if (Phase == -1)
	{
		if (CurrentMap ~= "01_NYC_UNATCOIsland")
			Phase = 0;
		return;
	}

	if (Phase == 1 && CapTime > 1.0)
	{
		Beeps(P);
		Note("beeps: the ini's factor");
		Phase = 2;
	}
	else if (Phase == 2 && CapTime > 6.0)
	{
		P.ConsoleCommand("set ini:Engine.Engine.AudioDevice AmbientFactor " $ Factor);
		Note("set the factor " $ Factor $ ", get " $ P.ConsoleCommand("get ini:Engine.Engine.AudioDevice AmbientFactor"));
		Beeps(P);
		Phase = 3;
	}
	else if (Phase == 3 && CapTime > 11.0)
	{
		P.ConsoleCommand("set ini:Engine.Engine.AudioDevice AmbientFactor 0.7");
		Note("set the factor 0.7, get " $ P.ConsoleCommand("get ini:Engine.Engine.AudioDevice AmbientFactor"));
		Beeps(P);
		Phase = 4;
	}
	else if (Phase == 4 && CapTime > 16.0)
	{
		Note("done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}
