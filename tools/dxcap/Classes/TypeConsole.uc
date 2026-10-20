//=============================================================================
// TypeConsole: text typed into an edit field (docs/DEVELOPMENT.md, scripted
// runs). From the menu map, the main menu shown, an edit field is put on it
// and given the focus; tools/dxcap/timelines/type.txt types into it as the
// window sends typed text, several characters in one go and some past 7-bit
// ASCII; then its text is logged ("DXTYPE:"), each character by its code.
// Then an exit.
//=============================================================================
class TypeConsole extends Console;

var float RunTime;
var int Step;
var EditWindow Edit;

function LogText(string Label)
{
	local string T, Codes;
	local int i;

	T = Edit.GetText();
	for (i = 0; i < Len(T); i++)
		Codes = Codes $ " " $ Asc(Mid(T, i, 1));
	Log("DXTYPE: " $ Label $ ": " $ Len(T) $ " characters," $ Codes);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local Window Holder;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		break;
	case 1:
		if (RunTime < 4.0)
			return;
		Holder = Root.GetTopWindow().NewChild(class'Window');
		Holder.SetPos(10, 10);
		Holder.SetSize(300, 40);
		Edit = EditWindow(Holder.NewChild(class'EditWindow'));
		Edit.SetSize(200, 20);
		Root.SetFocusWindow(Edit);
		Log("DXTYPE: an edit field, the focus " $ (Root.GetFocusWindow() == Edit));
		break;
	case 2:
		if (RunTime < 14.0)
			return;
		LogText("typed");
		Log("DXTYPE: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
