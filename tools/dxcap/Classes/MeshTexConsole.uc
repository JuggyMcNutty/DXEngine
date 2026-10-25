//=============================================================================
// MeshTexConsole: Actor.GetMeshTexture's answers, logged with "DXMESHTEX:"
// (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a Lamp1 spawned over the player and
// asked GetMeshTexture for -1 to 8, with no skins, then with its Skin set,
// then with MultiSkins[1] and [7] set too, and once more with Skin None; a
// CapJunk, which has no mesh, asked for 0 and 3; and an exit. What is
// compared between the engines: every line, exactly.
//=============================================================================
class MeshTexConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXMESHTEX: " $ S);
}

function Ask(string When, Actor A)
{
	local int i;
	local string S;

	for (i = -1; i <= 8; i++)
		S = S $ " " $ i $ ":" $ A.GetMeshTexture(i);
	Say(When $ ":" $ S);
}

function Probes()
{
	local Actor L, J;
	local Texture A, B, C;

	L = Viewport.Actor.Spawn(class'Lamp1',,, Viewport.Actor.Location + vect(0, 0, 200));
	J = Viewport.Actor.Spawn(class'CapJunk',,, Viewport.Actor.Location + vect(0, 0, 300));
	A = Texture(DynamicLoadObject("CoreTexMisc.Glass.WhiteNoise_A01", class'Texture'));
	B = Texture(DynamicLoadObject("CoreTexMisc.Glass.Type_Grn_A01", class'Texture'));
	C = Texture(DynamicLoadObject("CoreTexMisc.Glass.radar_A00", class'Texture'));
	if (L == None || J == None || A == None || B == None || C == None)
	{
		Say("not all made: " $ L $ " " $ J $ " " $ A $ " " $ B $ " " $ C);
		return;
	}
	Say("lamp mesh " $ L.Mesh $ " skin " $ L.Skin $ " texture " $ L.Texture);
	Ask("no skins", L);
	L.Skin = A;
	Ask("Skin set", L);
	L.MultiSkins[1] = B;
	L.MultiSkins[7] = C;
	Ask("MultiSkins 1 and 7 set too", L);
	L.Skin = None;
	Ask("Skin None again", L);
	Say("no mesh: 0:" $ J.GetMeshTexture(0) $ " 3:" $ J.GetMeshTexture(3));
	L.Destroy();
	J.Destroy();
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes();
		Say("done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
