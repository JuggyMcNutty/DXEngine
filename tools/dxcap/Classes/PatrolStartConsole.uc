//=============================================================================
// PatrolStartConsole: how Liberty Island's security bots start their patrol,
// tick by tick. A ScriptedPawn starts in StartUp, whose code sleeps
// FRand()+0.2 s and then follows its orders, and whose Tick follows them at
// once when the pawn was drawn within the last second (DeusEx.u): a pawn
// drawn in the first frame is on its way at the second tick.
//
// Liberty Island opened from wherever the run starts; from the console's
// first tick there, each tick until the actors' own time passes Seconds (3):
// the tick's number, the level's time and its step since the last tick, the
// actors' time (each step held to 0.4 s, as both engines' level tick holds
// the actors' step, so the first takes 0.4 s at most of a load's frame) and
// the console's own clock since the open; then each pawn named in Pawns
// (SecurityBot0,SecurityBot1), in that order: its state, MoveTarget,
// Destination, LatentFloat (a StartUp's sleep left), whether it was drawn
// within the last second (StartUp's test), its physics and place. Each line
// starts "DXPATROL:"; an exit after.
//
// What is compared, at DXCAP_FPS=120 so that every tick after the first is
// alike: by tick and by the actors' time, each pawn's tick out of StartUp
// and its first MoveTarget and Destination. The level's time and the
// console's clock at the first tick take in the load whole, the original's
// the longer: 1 s by the console's clock is the original's first tick,
// before any pawn has left StartUp, and some 0.6 s of the actors' time on
// in DXEngine, so NavReachConsole and SecondWayConsole probe at the first
// tick (bugs.md AI.16).
//=============================================================================
class PatrolStartConsole extends Console;

var config string Pawns;
var config float Seconds;

var float MapTime;
var float LastLevelTime;
var float ActorTime;
var int Ticks;
var int Phase;

function string V(vector X)
{
	return int(X.X) $ "," $ int(X.Y) $ "," $ int(X.Z);
}

function bool OnIsland(PlayerPawn P)
{
	return InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0;
}

function LogPawn(ScriptedPawn S)
{
	local string MT, Drawn;

	MT = "None";
	if (S.MoveTarget != None)
		MT = string(S.MoveTarget.Name);
	Drawn = "0";
	if (S.LastRendered() <= 1.0)
		Drawn = "1";
	Log("DXPATROL: tick " $ Ticks $ " " $ S.Name $ " state " $ S.GetStateName()
		$ " mt " $ MT $ " dest " $ V(S.Destination)
		$ " latent " $ S.LatentFloat $ " drawn " $ Drawn
		$ " physics " $ int(S.Physics) $ " at " $ V(S.Location));
}

// Each pawn of Pawns, in the list's order.
function LogPawns(PlayerPawn P)
{
	local ScriptedPawn S;
	local string Rest, One;
	local int i;

	Rest = Pawns;
	while (Rest != "")
	{
		i = InStr(Rest, ",");
		if (i < 0)
		{
			One = Rest;
			Rest = "";
		}
		else
		{
			One = Left(Rest, i);
			Rest = Mid(Rest, i + 1);
		}
		foreach P.AllActors(class'ScriptedPawn', S)
		{
			if (string(S.Name) ~= One)
			{
				LogPawn(S);
				break;
			}
		}
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local float Step;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	MapTime += Delta;

	if (Phase == 0)
	{
		if (!OnIsland(P))
		{
			if (MapTime > 3.0)
			{
				Log("DXPATROL: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = 1;
			}
			return;
		}
		Phase = 1;
	}
	if (Phase == 1 && OnIsland(P))
	{
		Ticks++;
		Step = P.Level.TimeSeconds - LastLevelTime;
		LastLevelTime = P.Level.TimeSeconds;
		ActorTime += FMin(Step, 0.4);
		Log("DXPATROL: tick " $ Ticks $ " time " $ P.Level.TimeSeconds $ " step " $ Step
			$ " actors " $ ActorTime $ " console " $ MapTime);
		LogPawns(P);
		if (ActorTime > Seconds)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 2;
		}
	}
}

defaultproperties
{
	Pawns="SecurityBot0,SecurityBot1"
	Seconds=3.0
}
