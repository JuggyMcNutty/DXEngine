//=============================================================================
// ScriptedConsole: ScriptedTexture.ReplaceTexture, which no script of the
// game calls (docs/DEVELOPMENT.md, scripted runs), on the harness's own
// scripted texture: tools/dxcap/testtextures.py's CapScripted, 64 by 64 in a
// palette of 256 greys, with no source texture or notify actor. From wherever
// the run starts, 2 s in, CapScripted drawn over the view at four times its
// size, at 64, 64, a step every 2 s, each marked and shot as CaptureConsole
// marks its shots: as loaded; replaced by CoreTexMisc.plaque_b (64 by 64, its
// pixels then greys by their indices); asked to take Compu_A04 (128 by 128)
// and None, each refused and logged; replaced by WhiteNoise_A01, an animated
// one, its current frame logged. Beside it, at 384, 64, CapSourced, the same
// with plaque_b its source texture: its source set to WhiteNoise_A01, shot;
// replaced by radar_A15, shot; then an exit. Each line starts "DXSCRIPTED:".
//=============================================================================
class ScriptedConsole extends Console;

#exec OBJ LOAD FILE=Textures\CapTextures.utx PACKAGE=DXCapture

var float RunTime, StepTime;
var int Step;
var int MarkShot;
var ScriptedTexture Scripted, Sourced;

function Shoot(PlayerPawn P, string What)
{
	Log("DXSCRIPTED: shot " $ Step $ ": " $ What);
	P.ConsoleCommand("shot");
	MarkShot = 0;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Texture Plaque, Compu, Noise, Radar;

	Super.Tick(Delta);
	if (Step == 99 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Step == 0)
	{
		if (RunTime < 2.0)
			return;
		Scripted = ScriptedTexture(DynamicLoadObject("DXCapture.CapScripted", class'ScriptedTexture'));
		Sourced = ScriptedTexture(DynamicLoadObject("DXCapture.CapSourced", class'ScriptedTexture'));
		if (Scripted == None || Sourced == None)
		{
			Log("DXSCRIPTED: no CapScripted or CapSourced, exiting");
			P.ConsoleCommand("exit");
			Step = 99;
			return;
		}
		Log("DXSCRIPTED: CapScripted " $ Scripted.USize $ " by " $ Scripted.VSize $ ", palette " $ Scripted.Palette);
		Log("DXSCRIPTED: CapSourced " $ Sourced.USize $ " by " $ Sourced.VSize $ ", palette " $ Sourced.Palette $ ", source " $ Sourced.SourceTexture);
		StepTime = RunTime;
		Step = 1;
		return;
	}
	if (RunTime - StepTime < 1.4)
		return;
	MarkShot = Step;
	if (RunTime - StepTime < 2.0)
		return;
	StepTime = RunTime;

	Plaque = Texture(DynamicLoadObject("CoreTexMisc.plaque_b", class'Texture'));
	Compu = Texture(DynamicLoadObject("CoreTexMisc.Compu_A04", class'Texture'));
	Noise = Texture(DynamicLoadObject("CoreTexMisc.WhiteNoise_A01", class'Texture'));
	Radar = Texture(DynamicLoadObject("CoreTexMisc.radar_A15", class'Texture'));
	if (Step == 1)
	{
		Shoot(P, "as loaded");
		Scripted.ReplaceTexture(Plaque);
		Log("DXSCRIPTED: replaced by " $ Plaque $ ", " $ Plaque.USize $ " by " $ Plaque.VSize);
	}
	else if (Step == 2)
	{
		Shoot(P, "after plaque_b");
		Log("DXSCRIPTED: asking for " $ Compu $ ", " $ Compu.USize $ " by " $ Compu.VSize);
		Scripted.ReplaceTexture(Compu);
		Log("DXSCRIPTED: asking for None");
		Scripted.ReplaceTexture(None);
	}
	else if (Step == 3)
	{
		Shoot(P, "after Compu_A04 and None");
		Scripted.ReplaceTexture(Noise);
		Log("DXSCRIPTED: replaced by " $ Noise $ ", its current frame " $ Noise.AnimCurrent);
	}
	else if (Step == 4)
	{
		Shoot(P, "after WhiteNoise_A01");
		Sourced.SourceTexture = Noise;
		Log("DXSCRIPTED: CapSourced's source set to " $ Sourced.SourceTexture);
	}
	else if (Step == 5)
	{
		Shoot(P, "CapSourced after its source set");
		Sourced.ReplaceTexture(Radar);
		Log("DXSCRIPTED: CapSourced replaced by " $ Radar $ ", " $ Radar.USize $ " by " $ Radar.VSize);
	}
	else if (Step == 6)
	{
		Shoot(P, "CapSourced after radar_A15");
		Log("DXSCRIPTED: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	}
	Step++;
}

event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (Scripted != None && Step != 99)
	{
		C.Style = 1;
		C.DrawColor.R = 255;
		C.DrawColor.G = 255;
		C.DrawColor.B = 255;
		C.SetPos(64, 64);
		C.DrawTile(Scripted, 256, 256, 0, 0, 64, 64);
		C.SetPos(384, 64);
		C.DrawTile(Sourced, 256, 256, 0, 0, 64, 64);
	}
	if (MarkShot == 0)
		return;
	// The original's grabber keeps the frames marked as CaptureConsole marks
	// them: a magenta block, then the shot's number in eight blocks.
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
