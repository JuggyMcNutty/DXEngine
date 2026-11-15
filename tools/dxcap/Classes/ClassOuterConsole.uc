//=============================================================================
// ClassOuterConsole: a class's own Outer, Name and Class against its
// defaults' (bugs.md VM.34), logged with "DXCLASSOUTER:". 2 s in, from
// wherever the run starts: the Outer of a class named in the script, of the
// player's class and of one DynamicLoadObject finds, its package's own Outer
// and Class, the "<package>.<class>" HX's Host Game screen builds from them;
// then the defaults' Outer, Name and Class; then an exit.
//
// The original's class is an object in its package (its UObject's Outer),
// while its default object's UObject part is zeroed but for its Class, the
// class (UObject::InitClassDefaultObject).
//=============================================================================
class ClassOuterConsole extends Console;

var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	local class<Object> C;

	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	Log("DXCLASSOUTER: DeusExPlayer's Outer " $ class'DeusEx.DeusExPlayer'.Outer $ ", its name " $ class'DeusEx.DeusExPlayer'.Outer.Name);
	Log("DXCLASSOUTER: Actor's Outer " $ class'Engine.Actor'.Outer $ ", Object's " $ class'Core.Object'.Outer $ ", Package's " $ class'Core.Package'.Outer);
	Log("DXCLASSOUTER: the player's class " $ Viewport.Actor.Class $ ", its Outer " $ Viewport.Actor.Class.Outer);
	C = class<Object>(DynamicLoadObject("DeusEx.MedKit", class'Class'));
	Log("DXCLASSOUTER: DynamicLoadObject's " $ C $ ", its Outer " $ C.Outer);
	Log("DXCLASSOUTER: the package's Outer " $ class'DeusEx.DeusExPlayer'.Outer.Outer $ ", its Class " $ class'DeusEx.DeusExPlayer'.Outer.Class);
	Log("DXCLASSOUTER: the Host Game screen's name " $ string(C.Outer.Name) $ "." $ string(C.Name));
	Log("DXCLASSOUTER: the class's Name " $ C.Name $ ", its Class " $ C.Class);
	Log("DXCLASSOUTER: the defaults' Outer " $ class'DeusEx.MedKit'.Default.Outer $ ", Name " $ class'DeusEx.MedKit'.Default.Name $ ", Class " $ class'DeusEx.MedKit'.Default.Class);
	Log("DXCLASSOUTER: done, exiting");
	Viewport.Actor.ConsoleCommand("exit");
}
