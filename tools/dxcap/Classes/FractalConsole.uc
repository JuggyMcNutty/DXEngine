//=============================================================================
// FractalConsole (bugs.md PKG.47, PKG.48): a fire, wet, wave or ice
// texture of a package on the run's paths, loaded by its own class and drawn
// on the canvas each frame, so that the engine steps it. 3 s in on any map:
// DXCAP_CONFIG="TextureName=<pkg>.<texture> ClassName=Fire.WetTexture";
// its size logged with "DXFRACTAL:", 4 s of frames, an exit.
//=============================================================================
class FractalConsole extends Console;

var config string TextureName;
var config string ClassName;

var float RunTime;
var int Phase;
var Texture Tex;

event Tick(float Delta)
{
	local class<Texture> TexClass;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 3.0)
	{
		TexClass = class<Texture>(DynamicLoadObject(ClassName, class'Class'));
		if (TexClass != None)
			Tex = Texture(DynamicLoadObject(TextureName, TexClass));
		if (Tex == None)
			Log("DXFRACTAL: no " $ TextureName $ " of " $ ClassName $ " (" $ TexClass $ ")");
		else
			Log("DXFRACTAL: " $ Tex $ " of " $ Tex.Class $ ", " $ Tex.USize $ " by " $ Tex.VSize);
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 7.0)
	{
		Log("DXFRACTAL: 4 s of frames drawn, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 9;
	}
}

event PostRender(canvas C)
{
	Super.PostRender(C);
	if (Tex == None || Phase != 1)
		return;
	C.SetPos(64, 64);
	C.DrawIcon(Tex, 1.0);
}

defaultproperties
{
}
