//=============================================================================
// OldVerConsole: a sound and a texture of a package of version 62, loaded
// beside their version-68 sources, logged with "DXOLDVER:"
// (docs/DEVELOPMENT.md, scripted runs; bugs.md PKG.40).
//
// The packages: vergen.py's CapMover62.uax and CapStucco62.utx, from the
// game's MoverSFX.uax and CoreTexStucco.utx with --version 62, in a folder a
// run finds by DXCAP_SERVERPKGS. On the start map, 2 s in, each pair loaded
// by DynamicLoadObject, the source first, each logged as it is asked and
// after: a sound by its GetSoundDuration, a texture by its USize, VSize and
// its first mip drawn for 1 s (a shot marked), then an exit.
// The original reads a sound's, a song's and each mip's lazy array with its
// skip offset from version 62 on (Engine.dll 0x10371140, the test at
// 0x1037118c), so each copy loads as its source: the same duration, 256 by
// 256 and the same picture. DXEngine reads the offset of a sound and a mip
// from 63 alone (Packages/Engine/Resources/USound.cpp:18,
// Packages/Engine/Resources/Textures/UTexture.cpp:19; a compressed mip's
// from 68, :63), so at 62 it takes the offset's bytes for the data's count:
// the copy's duration differs, or the load throws (the run's exit then the
// last line), where the original's matches its source's.
//=============================================================================
class OldVerConsole extends Console;

var float RunTime;
var int Phase;
var Texture Shown;

function Say(string S)
{
	Log("DXOLDVER: " $ S);
}

function TrySound(PlayerPawn P, string Path)
{
	local Sound S;

	Say("asking " $ Path);
	S = Sound(DynamicLoadObject(Path, class'Sound'));
	if (S == None)
		Say(Path $ " not loaded");
	else
		Say(Path $ " duration " $ P.GetSoundDuration(S));
}

function Texture TryTexture(string Path)
{
	local Texture T;

	Say("asking " $ Path);
	T = Texture(DynamicLoadObject(Path, class'Texture'));
	if (T == None)
		Say(Path $ " not loaded");
	else
		Say(Path $ " " $ T.USize $ "x" $ T.VSize);
	return T;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Phase == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		TrySound(P, "MoverSFX.Door.StallDoorOpen");
		TrySound(P, "CapMover62.Door.StallDoorOpen");
		TryTexture("CoreTexStucco.Stucco.RuffWhitStuco_B");
		Shown = TryTexture("CapStucco62.Stucco.RuffWhitStuco_B");
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 3.0)
	{
		Say("shot");
		P.ConsoleCommand("shot");
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 4.0)
	{
		Say("done, exiting");
		P.ConsoleCommand("exit");
		Phase = 99;
	}
}

// The mark for the original's screen grabber, as LoadingConsole's
function DrawMark(canvas C, int Number)
{
	local int i;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((Number & (1 << i)) != 0)
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

// The version-62 copy's first mip at 256 by 256 under the mark, while the
// shot is due
event PostRender(canvas C)
{
	Super.PostRender(C);
	if (Phase >= 1 && Phase < 99)
	{
		if (Shown != None)
		{
			C.Style = 1;
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
			C.SetPos(0, 16);
			C.DrawTile(Shown, 256, 256, 0, 0, Shown.USize, Shown.VSize);
		}
		DrawMark(C, 1);
	}
}
