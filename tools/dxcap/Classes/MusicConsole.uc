//=============================================================================
// MusicConsole: the music's level under the Music slider, its fades and the
// change after each, and a song's two sides, recorded in both engines
// (docs/DEVELOPMENT.md, scripted runs; docs/NATIVES.md, sound), on the
// harness's own song, tools/dxcap/testmusic.py's CapTone: steady tones, A
// (441 Hz) at order 0 and B (630 Hz) at order 2, centred, and at order 4 a
// pair, 551.25 Hz hard left and 882 Hz hard right. From the menu map, the
// player's dynamic music held in its outro (it then leaves the player's songs
// alone), the level's ambient sounds hushed and the music stopped; three
// beeps half a second apart, by which a recording is laid against the log;
// then, a beep a second before each step:
// 0. CapTone loaded straight into the pair's order, the slider at 255, 4 s:
//    Galaxy gives a song's channels their pans (and its speed and tempo)
//    only as its order count turns to order 0 (Galaxy.dll 0x10615710), its
//    load leaving every music voice centred (0x1060e850), so the original
//    plays both tones in the middle here, where DXEngine plays each to its
//    side, by choice (docs/NATIVES.md, sound).
// 1. tone A, order 0, with the Music slider at 255 (SetInstantMusicVolume,
//    as the Sound menu's slider sets it as it moves), 4 s; at 153, the
//    game's default, 4 s; at 64, 4 s; at 255 again, 3 s.
// 2. MTRAN_Fade from A to B, 5 s; MTRAN_FastFade from B to A, 5 s;
//    MTRAN_SlowFade from A to B, 8 s; the slider at 153, 3 s, and
//    MTRAN_Fade from B to A, 5 s: each a ClientSetMusic of CapTone at the
//    other tone's order, the change after the fade starting it.
// 3. the slider at 255 and the pair again, 5 s.
// 4. order 1, empty, from the pair, 4 s: Galaxy's section change silences
//    the voices and plays on from the order, so the original is silent,
//    where a replay of the song to it brings back tone A, struck at order 0
//    (Update, Galaxy.dll 0x1060a822; bugs.md AUD.16).
// 5. order 6, tone A on eight channels in phase, past full scale, 5 s, the
//    next step's beep over it: Galaxy writes the music out saturated at 16
//    bits and adds the sounds to it (0x106146a0), so the beep is heard
//    whole over the clipped tone, where over a tone past full scale the
//    clip takes part of it.
// 6. order 11, the pair centred by X40, which plays its 64 rows, ends the
//    song and loops to order 0, 13 s: Galaxy enters order 0 with each
//    channel's own pan again (0x10615710), the pair hard left and hard
//    right after the loop.
// 7. the music stopped, then CapTone loaded into order 8, tone A in rows of
//    2.42 s (speed 32, tempo 33), and a second on a jump to order 2, tone B,
//    4 s; then, early in order 2's second row, a jump to order 0, tone A,
//    5 s: Galaxy silences the voices at once and starts the section as the
//    playing row ends (0x10616f29), some 1.4 and 2.3 s later, SongSection
//    the order before it until then, 255 before order 0.
// 8. order 9, surround, 5 s: tone 551.25 Hz on a channel the header pans as
//    IT's surround, a quarter left in Galaxy's player, and 882 Hz on one
//    XB4 pans, Galaxy's 8xx surround a quarter right (0x10628990,
//    0x10616eb0), each with its right side inverted.
// 9. order 40, past the song's last, 5 s: Galaxy refuses it, logged, but
//    silences the voices and goes on at the next order, 10, as the row
//    ends (glxControlMusic, 0x1060d0e0).
// 10. order 10, tone A at C-2, 55.125 Hz, each of its frames eight output
//    frames, 5 s: read between frames in stereo, at its whole frame in
//    Galaxy's mono mix (UseStereo off), its image at 5,457 Hz 40 dB under.
//    SongSection is logged at each change from 7. to here.
// 11. each of Songs (Package.Name, by commas), stopped 1.5 s before and
//    played from order 0 at the slider's 255, SongSeconds (8 s): the stock
//    songs' own levels, as each engine's player renders them; long enough,
//    a song through its loop. Then an exit.
// Each step is logged with "DXMUSIC:" and the console's clock: "beep",
// "segment <slider> <tone>", "fade <transition> <seconds> <from> <to>
// <slider>", "stereo <slider> [first]", "section <slider> <order>", "loud
// <slider>", "loop <slider>", "slow <slider>", "late <slider> <order>
// <from> <to>", "surround <slider>", "refused <slider> <order>", "low
// <slider>", "songsection <order>", "stop", "song <name>", "end";
// tools/dxcap/sound.py --music
// reads the run's recording against them (DXCAP_RECORD=1 DXCAP_MUSIC=1).
//=============================================================================
class MusicConsole extends Console;

#exec OBJ LOAD FILE=Music\CapMusic.umx PACKAGE=DXCapture

var config string Songs;
var config float SongSeconds;

var float CapTime, SongAt;
var int Step, LastSection;
var bool bWatch;
var string Pending;
var bool bBeeped;
var Music Tone;
var Object Stopped;

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
	Log("DXMUSIC: " $ What $ " at " $ CapTime);
}

function Beep(PlayerPawn P)
{
	P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_Misc, 1.0, false, 65536.0);
	Note("beep");
}

// True once CapTime reaches At, the step's beep a second before.
function bool Due(PlayerPawn P, float At)
{
	if (!bBeeped && CapTime >= At - 1.0)
	{
		Beep(P);
		bBeeped = true;
	}
	if (CapTime < At)
		return false;
	bBeeped = false;
	return true;
}

function Segment(PlayerPawn P, byte Slider, string ToneName)
{
	P.SetInstantMusicVolume(Slider);
	Note("segment " $ Slider $ " " $ ToneName);
}

// The transition itself is the caller's ClientSetMusic, its constant written
// out there, as the harness's consoles pass Actor's enums.
function Fade(string HowName, string Seconds, string From, string To, byte Slider)
{
	Note("fade " $ HowName $ " " $ Seconds $ " " $ From $ " " $ To $ " " $ Slider);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor A;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;
	P.ReducedDamageType = 'All';
	Silence(P);
	// The order the audio writes back into SongSection, at each change
	if (bWatch && P.SongSection != LastSection)
	{
		LastSection = P.SongSection;
		Note("songsection " $ LastSection);
	}

	if (Step == 0 && CapTime > 1.0)
	{
		Tone = Music(DynamicLoadObject("DXCapture.CapTone", class'Music'));
		if (Tone == None)
		{
			Note("no DXCapture.CapTone, exiting");
			P.ConsoleCommand("exit");
			Step = 99;
			return;
		}
		// DeusExPlayer's dynamic music held in its outro, where it leaves the
		// player's songs alone, as on an intro map (the level's Song is const)
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).musicMode = MUS_Outro;
		foreach P.AllActors(class'Actor', A)
			if (A.AmbientSound != None)
				A.AmbientSound = None;
		P.ClientSetMusic(None, 255, 255, MTRAN_Instant);
		Note("music stopped, the ini's MusicVolume " $ P.ConsoleCommand("get ini:Engine.Engine.AudioDevice MusicVolume")
			$ ", Latency " $ P.ConsoleCommand("get ini:Engine.Engine.AudioDevice Latency"));
		Step = 1;
	}
	else if (Step == 1 && CapTime > 2.0)
	{
		Beep(P);
		Step = 2;
	}
	else if (Step == 2 && CapTime > 2.5)
	{
		Beep(P);
		Step = 3;
	}
	else if (Step == 3 && CapTime > 3.0)
	{
		Beep(P);
		Step = 4;
	}
	else if (Step == 4 && CapTime > 4.0)
	{
		P.SetInstantMusicVolume(255);
		P.ClientSetMusic(Tone, 4, 255, MTRAN_Instant);
		Note("stereo 255 first");
		Step = 41;
	}
	else if (Step == 41 && Due(P, 8.0))
	{
		P.ClientSetMusic(Tone, 0, 255, MTRAN_Instant);
		Note("segment 255 A");
		Step = 5;
	}
	else if (Step == 5 && Due(P, 12.0))
	{
		Segment(P, 153, "A");
		Step = 6;
	}
	else if (Step == 6 && Due(P, 16.0))
	{
		Segment(P, 64, "A");
		Step = 7;
	}
	else if (Step == 7 && Due(P, 20.0))
	{
		Segment(P, 255, "A");
		Step = 8;
	}
	else if (Step == 8 && Due(P, 23.0))
	{
		P.ClientSetMusic(Tone, 2, 255, MTRAN_Fade);
		Fade("MTRAN_Fade", "1.0", "A", "B", 255);
		Step = 9;
	}
	else if (Step == 9 && Due(P, 28.0))
	{
		P.ClientSetMusic(Tone, 0, 255, MTRAN_FastFade);
		Fade("MTRAN_FastFade", "0.333", "B", "A", 255);
		Step = 10;
	}
	else if (Step == 10 && Due(P, 33.0))
	{
		P.ClientSetMusic(Tone, 2, 255, MTRAN_SlowFade);
		Fade("MTRAN_SlowFade", "5.0", "A", "B", 255);
		Step = 11;
	}
	else if (Step == 11 && Due(P, 41.0))
	{
		Segment(P, 153, "B");
		Step = 12;
	}
	else if (Step == 12 && Due(P, 44.0))
	{
		P.ClientSetMusic(Tone, 0, 255, MTRAN_Fade);
		Fade("MTRAN_Fade", "1.0", "B", "A", 153);
		Step = 13;
	}
	else if (Step == 13 && Due(P, 49.0))
	{
		P.SetInstantMusicVolume(255);
		P.ClientSetMusic(Tone, 4, 255, MTRAN_Instant);
		Note("stereo 255");
		Step = 14;
	}
	else if (Step == 14 && Due(P, 54.0))
	{
		P.ClientSetMusic(Tone, 1, 255, MTRAN_Instant);
		Note("section 255 1");
		Step = 141;
	}
	else if (Step == 141 && Due(P, 58.0))
	{
		P.ClientSetMusic(Tone, 6, 255, MTRAN_Instant);
		Note("loud 255");
		Step = 142;
	}
	else if (Step == 142 && Due(P, 63.0))
	{
		P.ClientSetMusic(Tone, 11, 255, MTRAN_Instant);
		Note("loop 255");
		Step = 143;
	}
	else if (Step == 143 && CapTime > 76.0)
	{
		P.ClientSetMusic(None, 255, 255, MTRAN_Instant);
		Note("stop");
		LastSection = -1;
		bWatch = true;
		Step = 144;
	}
	else if (Step == 144 && Due(P, 77.0))
	{
		P.ClientSetMusic(Tone, 8, 255, MTRAN_Instant);
		Note("slow 255");
		Step = 145;
	}
	else if (Step == 145 && Due(P, 78.0))
	{
		P.ClientSetMusic(Tone, 2, 255, MTRAN_Instant);
		Note("late 255 2 A B");
		Step = 146;
	}
	else if (Step == 146 && Due(P, 82.0))
	{
		P.ClientSetMusic(Tone, 0, 255, MTRAN_Instant);
		Note("late 255 0 B A");
		Step = 147;
	}
	else if (Step == 147 && Due(P, 87.0))
	{
		P.ClientSetMusic(Tone, 9, 255, MTRAN_Instant);
		Note("surround 255");
		Step = 148;
	}
	else if (Step == 148 && Due(P, 92.0))
	{
		P.ClientSetMusic(Tone, 40, 255, MTRAN_Instant);
		Note("refused 255 40");
		Step = 149;
	}
	else if (Step == 149 && Due(P, 97.0))
	{
		P.ClientSetMusic(Tone, 10, 255, MTRAN_Instant);
		Note("low 255");
		Step = 150;
	}
	else if (Step == 150 && CapTime > 102.0)
	{
		bWatch = false;
		Pending = Songs;
		SongAt = 104.0;
		Step = 15;
	}
	else if (Step == 15 && CapTime >= SongAt - 1.5)
	{
		// The last song played its SongSeconds too
		if (Pending == "")
		{
			Note("end");
			P.ConsoleCommand("exit");
			Step = 99;
		}
		else
		{
			P.ClientSetMusic(None, 255, 255, MTRAN_Instant);
			Note("stop");
			Step = 16;
		}
	}
	else if (Step == 16 && Due(P, SongAt))
	{
		PlaySong(P);
		SongAt += SongSeconds + 2.0;
		Step = 15;
	}
}

// The next of Songs, from order 0 at once, the slider full
function PlaySong(PlayerPawn P)
{
	local string Name;
	local Music Song;
	local int i;

	i = InStr(Pending, ",");
	if (i < 0)
	{
		Name = Pending;
		Pending = "";
	}
	else
	{
		Name = Left(Pending, i);
		Pending = Mid(Pending, i + 1);
	}
	Song = Music(DynamicLoadObject(Name, class'Music'));
	if (Song == None)
	{
		Note("no song " $ Name);
		return;
	}
	P.SetInstantMusicVolume(255);
	P.ClientSetMusic(Song, 0, 255, MTRAN_Instant);
	Note("song " $ Name);
}

defaultproperties
{
	Songs="Title_Music.Title_Music,LibertyIsland_Music.LibertyIsland_Music,UNATCO_Music.UNATCO_Music,VersaLife_Music.VersaLife_Music,NavalBase_Music.NavalBase_Music,BatteryPark_Music.BatteryPark_Music"
	SongSeconds=8.0
}
