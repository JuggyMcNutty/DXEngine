//=============================================================================
// Vm6Console: the VM's edge cases, logged with "DXVM6:", from the menu
// map. 2 s in, one case a tick, each between a "begin" and an "end" line so
// that what each engine logs for it (its warnings) can be counted between:
//
// - an: Accessed None through NoneProbe, a CapVm6Probe variable that stays
//   None, in each place a context can stand: a statement's own call (an1),
//   a Let's right side (an2), a condition (an3), an argument (an4), a Let's
//   left side (an5), a Let from a call (an6), += and ++ on a member
//   (an7 int +=, an8 float +=, an9 ++). The original's execContext
//   (Core.dll 0x101300d0) warns "Accessed None" at each.
// - oob: a static array of three read at 5 and -1 and written at 7, the
//   elements logged after, and what each engine logs for it.
// - deep: script call chains Depth deep (240 by default, under the 250 the
//   original allows with the Tick): a plain one returning a sum (deep1,
//   Sum(n-1) + 1), one returning a string (deep2, "x" $ Cat(n-1)), one
//   through the Spawn native at every level (deep3, CapVm6Spawn's
//   PostBeginPlay), and one expression 700 operators deep (deep4). Each
//   "end" line says the chain came back; a run that stops after a "begin"
//   names the case.
// - assert (only with [DXCapture.Vm6Console] bAssert=True, last): a failed
//   assert, which stops both engines: the text each gives.
//
// Then an exit.
//=============================================================================
class Vm6Console extends Console;

var config int Depth;
var config bool bAssert;
var float RunTime;
var int Phase;
var CapVm6Probe NoneProbe;
var int Arr[3];

function Say(string S)
{
	Log("DXVM6: " $ S);
}

function int Sum(int n)
{
	if (n <= 0)
		return 0;
	return Sum(n - 1) + 1;
}

function string Cat(int n)
{
	if (n <= 0)
		return "";
	return "x" $ Cat(n - 1);
}

function string LongCat()
{
		return "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $
			"a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a" $ "a";
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local int I, n;
	local float F;
	local string S;
	local CapVm6Spawn Sp;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	n = Depth;
	if (n <= 0)
		n = 240;

	if (Phase == 0)
	{
		if (RunTime < 2.0)
			return;
		Say("an1 begin");
		NoneProbe.Nudge();
		Say("an1 end");

		I = 3;
		Say("an2 begin");
		I = NoneProbe.Count;
		Say("an2 end " $ I);

		I = 3;
		Say("an3 begin");
		if (NoneProbe.bFlag)
			I = 1;
		Say("an3 end " $ I);

		Say("an4 begin");
		S = "v" $ NoneProbe.Count;
		Say("an4 end " $ S);

		Say("an5 begin");
		NoneProbe.Count = 5;
		Say("an5 end");

		I = 3;
		Say("an6 begin");
		I = NoneProbe.Get();
		Say("an6 end " $ I);

		Say("an7 begin");
		NoneProbe.Count += 1;
		Say("an7 end");

		Say("an8 begin");
		NoneProbe.Energy += 1.0;
		Say("an8 end");

		I = 3;
		Say("an9 begin");
		I = NoneProbe.Count++;
		Say("an9 end " $ I);

		Arr[0] = 10;
		Arr[1] = 11;
		Arr[2] = 12;
		Say("oob begin");
		I = 5;
		Say("oob read5 " $ Arr[I]);
		I = -1;
		Say("oob readneg " $ Arr[I]);
		I = 7;
		Arr[I] = 99;
		Say("oob end " $ Arr[0] $ "," $ Arr[1] $ "," $ Arr[2]);
		Phase = 1;
	}
	else if (Phase == 1)
	{
		Say("deep1 begin " $ n);
		I = Sum(n);
		Say("deep1 end " $ I);
		Phase = 2;
	}
	else if (Phase == 2)
	{
		Say("deep2 begin " $ n);
		S = Cat(n);
		Say("deep2 end " $ Len(S));
		Phase = 3;
	}
	else if (Phase == 3)
	{
		class'CapVm6Spawn'.default.Left = n - 1;
		class'CapVm6Spawn'.default.Reached = 0;
		Say("deep3 begin " $ n);
		Sp = P.Spawn(class'CapVm6Spawn');
		Say("deep3 end " $ class'CapVm6Spawn'.default.Reached);
		foreach P.AllActors(class'CapVm6Spawn', Sp)
			Sp.Destroy();
		Phase = 4;
	}
	else if (Phase == 4)
	{
		Say("deep4 begin");
		S = LongCat();
		Say("deep4 end " $ Len(S));
		Phase = 5;
	}
	else if (Phase == 5)
	{
		if (bAssert)
		{
			Say("assert begin");
			I = 3;
			assert(I == 4);
			Say("assert end");
		}
		Say("done");
		P.ConsoleCommand("exit");
		Phase = 6;
	}
}
