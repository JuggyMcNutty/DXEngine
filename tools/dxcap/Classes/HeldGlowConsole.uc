//=============================================================================
// HeldGlowConsole (bugs.md REN.46; HeldConsole's structure): what lights
// the unlit faces of an item a pawn holds. The player seen from behind
// (bBehindView) on Liberty Island holding the assault gun, its muzzle
// flash's slot (MultiSkins[2], a PF_Unlit translucent two-sided material of
// AssaultGun3rd) given Texture'FlatFXTex34' as DeusExWeapon's
// SwapMuzzleFlashTexture gives it, every tick so nothing clears it. Shot 0:
// the player at its own ScaleGlow (1). Then the player made as a cloaked
// NPC is made (ScriptedPawn.EnableCloak -> SetSkinStyle: STY_Translucent,
// ScaleGlow 0.05). Shot 1: the gun drawn in the player's style.
// Render.dll's DrawLodMesh (0x10b0f6e0) takes the unlit faces' level from
// the mesh drawn, the gun: its AmbientGlow / 256 + its ScaleGlow / 2 (0.5);
// DXEngine's VertexLight::UnlitLevel takes the holder's (0.025). Each shot
// marked as HeldConsole marks it; "DXHELDGLOW:" lines log both actors'
// AmbientGlow, ScaleGlow and Style. Compare shots.py --region over the gun's
// muzzle, at DXCAP_WINDOW=1024x768, the original with DXCAP_RENDERER=D3D.
//=============================================================================
class HeldGlowConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var int ShotCount;
var Inventory Gun;

var int MarkShot;
var bool bMarking;
var float MarkTime;
var bool bShotPending;

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

function Shot(PlayerPawn P, string Label)
{
	Log("DXHELDGLOW: shot " $ ShotCount $ " = " $ Label);
	MarkShot = ShotCount;
	bMarking = true;
	MarkTime = 0;
	bShotPending = true;
}

function TickShot(PlayerPawn P, float Delta)
{
	if (!bShotPending)
		return;
	MarkTime += Delta;
	if (MarkTime > 0.8)
	{
		P.ConsoleCommand("shot");
		bMarking = false;
		bShotPending = false;
		ShotCount++;
	}
}

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

// As HeldConsole's GiveItem: the game's own pickup, its ammo beside it
function Inventory GiveItem(PlayerPawn P, class<Inventory> ItemClass)
{
	local DeusExPlayer DXP;
	local Inventory item;
	local Ammo ammo;

	DXP = DeusExPlayer(P);
	item = P.Spawn(ItemClass,,, P.Location + vect(40,0,0));
	if (item == None)
	{
		Log("DXHELDGLOW: could not spawn " $ ItemClass);
		return None;
	}
	if (DeusExWeapon(item) != None && DeusExWeapon(item).AmmoType == None && DeusExWeapon(item).AmmoName != None)
	{
		ammo = P.Spawn(DeusExWeapon(item).AmmoName);
		if (ammo != None)
		{
			DXP.AddInventory(ammo);
			ammo.BecomeItem();
			ammo.AmmoAmount = DeusExWeapon(item).PickUpAmmoCount;
			ammo.GotoState('Idle2');
		}
	}
	item.GiveTo(DXP);
	Log("DXHELDGLOW: gave " $ item.Name);
	return item;
}

function int PosOf(PlayerPawn P, Inventory item)
{
	local DeusExPlayer DXP;
	local int i;

	DXP = DeusExPlayer(P);
	for (i = 0; i < 10; i++)
		if (DeusExRootWindow(DXP.rootWindow).hud.belt.objects[i].GetItem() == item)
			return i;
	return -1;
}

function LogGlow(PlayerPawn P, string Label)
{
	local DeusExPlayer DXP;
	DXP = DeusExPlayer(P);
	Log("DXHELDGLOW: " $ Label $ ": weapon " $ DXP.Weapon $ " AmbientGlow " $ Gun.AmbientGlow $ " ScaleGlow " $ Gun.ScaleGlow
	    $ " Style " $ Gun.Style $ "; player AmbientGlow " $ P.AmbientGlow $ " ScaleGlow " $ P.ScaleGlow $ " Style " $ P.Style
	    $ "; skin 2 " $ Gun.MultiSkins[2]);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (Gun != None)
		Gun.MultiSkins[2] = Texture'FlatFXTex34';
	TickShot(P, Delta);
	if (bShotPending)
		return;

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 6.0)
		{
			P.SetPhysics(PHYS_Walking);
			Gun = GiveItem(P, class'WeaponAssaultGun');
			P.bBehindView = true;
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2 && StepTime > 1.0)
	{
		DXP.ActivateBelt(PosOf(P, Gun));
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 2.5)
	{
		LogGlow(P, "the gun's flash slot set, the player as it is");
		Shot(P, "the flash, the player at its own glow");
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 1.0)
	{
		// As ScriptedPawn.SetSkinStyle(STY_Translucent, ..., 0.05) leaves a
		// cloaked NPC, the skins aside
		P.Style = STY_Translucent;
		P.ScaleGlow = 0.05;
		StepTime = 0;
		Phase = 5;
	}
	else if (Phase == 5 && StepTime > 1.0)
	{
		LogGlow(P, "the player cloaked");
		Shot(P, "the flash, the player cloaked");
		StepTime = 0;
		Phase = 6;
	}
	else if (Phase == 6 && StepTime > 1.0)
	{
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 7;
	}
}
