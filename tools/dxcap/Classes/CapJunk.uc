//=============================================================================
// CapJunk: JunkConsole's actor, a public property of each kind for text
// that is no value of it to be set on.
//=============================================================================
class CapJunk extends Actor;

var int I;
var float F;
var bool B;
var byte Bt;
var string S;
var name N;

function string Values()
{
	return "int " $ I $ " float " $ F $ " bool " $ B $ " byte " $ Bt $ " string [" $ S $ "] name " $ N;
}

defaultproperties
{
	I=5
	F=1.5
	B=True
	Bt=7
	S="kept"
	N=Kept
}
