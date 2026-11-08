//=============================================================================
// ClipWrapConsole: whether the arrow keys' focus moves wrap at a clip
// window's ends, logged with "DXCLIPWRAP:" (docs/DEVELOPMENT.md, scripted
// runs; bugs.md EXT.30). The original's MoveFocusLeft, Right, Up and Down
// (Extension.dll 0x100033b0..0x100033e0) ask XWindow::MoveFocus
// (0x1004ef30) not to wrap, so the focus window's tab group decides: every
// tab group wraps (bWrapFocus, set by XTabGroupWindow::Init 0x10044d90)
// but a clip window, whose Init (0x1000b320) clears it; at a clip window's
// end the focus stays where it is. The root's script sends the arrow keys
// no window takes to these (RootWindow.VirtualKeyPressed), as the persona
// screens' item buttons leave them (PersonaItemButton.VirtualKeyPressed):
// the inventory screen's ammo buttons, the medical bot's augmentation
// buttons, each in a clip window.
//
// On the menu map, 3 s in: a modal of its own (focus by clicks alone, at
// 20, 20, 400 by 400) holding a clip window at 10, 10, 200 by 300 whose
// child, 200 by 300, holds buttons C1, C2 and C3 at 10, 0, 100 and 200, and
// a tab group at 220, 10, 150 by 300 holding T1, T2 and T3 the same -- each
// button 50 by 20. Then, 0.5 s apart, the focus put on a button and moved,
// the focus after each logged: C3 down, C3 right, C1 up, C1 left, C2 down
// (the control, within the clip), T3 down, T1 up (a plain tab group, which
// wraps in both). Then the modal is destroyed and the run exits.
//
// What differs: the original's focus stays on C3, C3, C1 and C1 at the
// clip's ends; DXEngine's before a fix goes round to C1, C1, C3 and C3. C2
// down gives C3, T3 down T1 and T1 up T3 in both.
//=============================================================================
class ClipWrapConsole extends Console;

var float RunTime;
var float StepTime;
var int Phase;
var int Step;
var ModalWindow Modal;
var ClipWindow Clip;
var TabGroupWindow Group;
var ButtonWindow C1, C2, C3, T1, T2, T3;

function string NameOf(Window W)
{
	if (W == C1) return "C1";
	if (W == C2) return "C2";
	if (W == C3) return "C3";
	if (W == T1) return "T1";
	if (W == T2) return "T2";
	if (W == T3) return "T3";
	if (W == None) return "None";
	return string(W.Class.Name);
}

function ButtonWindow AddButton(Window Holder, float X, float Y)
{
	local ButtonWindow B;

	B = ButtonWindow(Holder.NewChild(class'ButtonWindow'));
	B.SetPos(X, Y);
	B.SetSize(50, 20);
	return B;
}

// Step n: the start and the move; the move's direction 0 left, 1 right, 2
// up, 3 down
function bool StepOf(int n, out ButtonWindow Start, out int Dir, out string What)
{
	switch (n)
	{
	case 0: Start = C3; Dir = 3; What = "C3 down"; return true;
	case 1: Start = C3; Dir = 1; What = "C3 right"; return true;
	case 2: Start = C1; Dir = 2; What = "C1 up"; return true;
	case 3: Start = C1; Dir = 0; What = "C1 left"; return true;
	case 4: Start = C2; Dir = 3; What = "C2 down"; return true;
	case 5: Start = T3; Dir = 3; What = "T3 down"; return true;
	case 6: Start = T1; Dir = 2; What = "T1 up"; return true;
	}
	return false;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local Window Holder;
	local ButtonWindow Start;
	local int Dir;
	local string What;

	Super.Tick(Delta);
	if (Phase == 9 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	StepTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (Root == None)
		return;
	if (Phase == 0 && RunTime > 3.0)
	{
		Modal = ModalWindow(Root.NewChild(class'ModalWindow'));
		Modal.SetMouseFocusMode(MFOCUS_Click);
		Modal.SetPos(20, 20);
		Modal.SetSize(400, 400);
		Clip = ClipWindow(Modal.NewChild(class'ClipWindow'));
		Clip.SetPos(10, 10);
		Clip.SetSize(200, 300);
		Holder = Clip.NewChild(class'Window');
		Holder.SetSize(200, 300);
		C1 = AddButton(Holder, 10, 0);
		C2 = AddButton(Holder, 10, 100);
		C3 = AddButton(Holder, 10, 200);
		Group = TabGroupWindow(Modal.NewChild(class'TabGroupWindow'));
		Group.SetPos(220, 10);
		Group.SetSize(150, 300);
		T1 = AddButton(Group, 10, 0);
		T2 = AddButton(Group, 10, 100);
		T3 = AddButton(Group, 10, 200);
		Log("DXCLIPWRAP: the modal made");
		Step = 0;
		StepTime = 0;
		Phase = 1;
	}
	else if (Phase == 1 && StepTime > 0.5)
	{
		if (!StepOf(Step, Start, Dir, What))
		{
			Modal.Destroy();
			Modal = None;
			StepTime = 0;
			Phase = 3;
			return;
		}
		Modal.SetFocusWindow(Start);
		StepTime = 0;
		Phase = 2;
	}
	else if (Phase == 2 && StepTime > 0.5)
	{
		StepOf(Step, Start, Dir, What);
		Log("DXCLIPWRAP: " $ What $ ": from " $ NameOf(Root.GetFocusWindow()));
		if (Dir == 0)
			Start.MoveFocusLeft();
		else if (Dir == 1)
			Start.MoveFocusRight();
		else if (Dir == 2)
			Start.MoveFocusUp();
		else
			Start.MoveFocusDown();
		Log("DXCLIPWRAP: " $ What $ ": focus " $ NameOf(Root.GetFocusWindow()));
		Step++;
		StepTime = 0;
		Phase = 1;
	}
	else if (Phase == 3 && StepTime > 0.5)
	{
		Log("DXCLIPWRAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
