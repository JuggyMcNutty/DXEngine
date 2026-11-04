//=============================================================================
// CapQuad: the harness's flat mesh (tools/dxcap/testmeshes.py): a square of
// 16 by 16 tiles lying flat, 1920 units across at a DrawScale of 1, each
// tile the whole of its texture once, unlit and touching nothing.
// MipConsole lays one under the view with CapMips for its skin.
//=============================================================================
class CapQuad extends Actor;

#exec MESH IMPORT MESH=CapQuad ANIVFILE=Models\CapQuad_a.3d DATAFILE=Models\CapQuad_d.3d X=0 Y=0 Z=0
#exec MESH ORIGIN MESH=CapQuad X=0 Y=0 Z=0
#exec MESH SEQUENCE MESH=CapQuad SEQ=All STARTFRAME=0 NUMFRAMES=1
#exec MESHMAP NEW MESHMAP=CapQuad MESH=CapQuad
#exec MESHMAP SCALE MESHMAP=CapQuad X=1.0 Y=1.0 Z=1.0

defaultproperties
{
	DrawType=DT_Mesh
	Mesh=Mesh'CapQuad'
	bUnlit=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
	bProjTarget=False
	Physics=PHYS_None
	RemoteRole=ROLE_None
}
