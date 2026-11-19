//=============================================================================
// LoadTwiceConsole (bugs.md PKG.57): one object of a package on the run's
// paths asked for twice by Object.DynamicLoadObject, 2 s in on any map:
// DXCAP_CONFIG="ObjectName=<pkg>.<object> ClassName=Engine.Texture"; each
// answer logged with "DXLOADTWICE:", a texture's size with it, an exit.
//=============================================================================
class LoadTwiceConsole extends Console;

var config string ObjectName;
var config string ClassName;

var float RunTime;
var bool bDone;

function Probe(string Which, class<Object> Cls)
{
	local Object O;

	O = DynamicLoadObject(ObjectName, Cls, true);
	if (O == None)
		Log("DXLOADTWICE: " $ Which $ " " $ ObjectName $ " = None");
	else if (Texture(O) != None)
		Log("DXLOADTWICE: " $ Which $ " " $ ObjectName $ " = " $ O.Class.Name $ " " $ O.Name $ ", " $ Texture(O).USize $ " by " $ Texture(O).VSize);
	else
		Log("DXLOADTWICE: " $ Which $ " " $ ObjectName $ " = " $ O.Class.Name $ " " $ O.Name);
}

event Tick(float Delta)
{
	local class<Object> Cls;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	Cls = class<Object>(DynamicLoadObject(ClassName, class'Class'));
	if (Cls == None)
		Log("DXLOADTWICE: no class " $ ClassName);
	else
	{
		Probe("first", Cls);
		Probe("second", Cls);
	}
	Log("DXLOADTWICE: done");
	Viewport.Actor.ConsoleCommand("exit");
}
