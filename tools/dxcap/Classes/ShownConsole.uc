//=============================================================================
// ShownConsole: Window.IsVisible and IsSensitive under a hidden or disabled
// parent, logged with "DXSHOWN:" (docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: a window of the console's own under
// the root, and a child under it, each asked with no argument, False and
// True while the parent is shown, hidden, shown again, disabled and enabled
// again, and the child hidden itself; then the HUD's message log the same
// way under the HUD hidden and disabled; and an exit. What is compared
// between the engines: every line, exactly.
//=============================================================================
class ShownConsole extends Console;

var float RunTime;
var bool bDone;

function Say(string S)
{
	Log("DXSHOWN: " $ S);
}

function Ask(string When, Window W)
{
	Say(When $ ": visible " $ W.IsVisible() $ " " $ W.IsVisible(False) $ " " $ W.IsVisible(True) $ "; sensitive " $ W.IsSensitive() $ " " $ W.IsSensitive(False) $ " " $ W.IsSensitive(True));
}

function Probes()
{
	local DeusExRootWindow Root;
	local Window Parent, Child;

	Root = DeusExRootWindow(DeusExPlayer(Viewport.Actor).rootWindow);
	if (Root == None)
	{
		Say("no root window");
		return;
	}
	Ask("root", Root);
	Parent = Root.NewChild(class'Window');
	if (Parent == None)
	{
		Say("no window made");
		return;
	}
	Child = Parent.NewChild(class'Window');
	Ask("parent shown, child", Child);
	Parent.Hide();
	Ask("parent hidden, parent", Parent);
	Ask("parent hidden, child", Child);
	Parent.Show();
	Ask("parent shown again, child", Child);
	Parent.DisableWindow();
	Ask("parent disabled, parent", Parent);
	Ask("parent disabled, child", Child);
	Parent.EnableWindow();
	Ask("parent enabled again, child", Child);
	Child.Hide();
	Ask("child hidden itself, child", Child);
	Child.Show();
	Child.DisableWindow();
	Ask("child disabled itself, child", Child);
	Child.EnableWindow();
	Root.Hide();
	Ask("root hidden, child", Child);
	Root.Show();
	Parent.Destroy();
	if (Root.hud == None || Root.hud.msgLog == None)
	{
		Say("no HUD");
		return;
	}
	Ask("hud shown, log", Root.hud.msgLog);
	Root.hud.Hide();
	Ask("hud hidden, log", Root.hud.msgLog);
	Root.hud.Show();
	Root.hud.DisableWindow();
	Ask("hud disabled, log", Root.hud.msgLog);
	Root.hud.EnableWindow();
	Ask("hud enabled again, log", Root.hud.msgLog);
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
