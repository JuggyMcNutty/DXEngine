//=============================================================================
// CapLightProbe (bugs.md AI.22): LightTreeConsole's probe, which
// works in its own Tick, in the level's tick, where the AI asks the light
// (AIVisibility, from AICanSee in the event manager's turns): Moved (a
// CapLightSwitch) switched on at its first tick; at its third, Off (another,
// still off) switched on and Moved sent 400 units along X, AIGetLightLevel
// asked 50 units from
// each before and after in the same tick, then again in the next three
// ticks. Logged with "DXLTREE:".
//
// The original's AIGetLightLevel (Engine.dll 0x1036b980) walks the level's
// actors at each call; DXEngine's AILightAt (Packages/Engine/Actors/
// UActor.cpp) asks the light tree the last frame's BeginFrame built
// (Light/LightSystem.cpp, LightTree), from the lights then on (LightType and
// brightness), each by its place and radius then, the distance then
// checked again with the light as it is now. Expected: the original's
// "after" lines count both lights at once (some 0.409 over the ambient for
// a white light 50 units in of 275); DXEngine's count neither in that tick
// -- Off not in the tree, Moved's box at its old place -- and both from the
// next.
//=============================================================================
class CapLightProbe extends Actor;

var CapLightSwitch Off, Moved;
var vector OffAt, MovedAt;
var int Ticks;

function Say(string S)
{
	Log("DXLTREE: " $ S);
}

function Probe(string Label)
{
	Say(Label $ " t " $ Level.TimeSeconds $ " off(" $ Off.LightType $ ") " $ AIGetLightLevel(OffAt + vect(50, 0, 0))
		$ " moved(" $ Moved.Location.X $ ") new " $ AIGetLightLevel(Moved.Location + vect(50, 0, 0))
		$ " old " $ AIGetLightLevel(MovedAt + vect(50, 0, 0)));
}

event Tick(float Delta)
{
	if (Off == None || Moved == None)
		return;
	Ticks++;
	if (Ticks == 1)
		Moved.LightType = LT_Steady;
	else if (Ticks == 3)
	{
		Probe("tick3 before");
		Off.LightType = LT_Steady;
		Moved.SetLocation(MovedAt + vect(400, 0, 0));
		Probe("tick3 after");
	}
	else if (Ticks > 3 && Ticks <= 6)
		Probe("tick" $ Ticks);
}

defaultproperties
{
	bHidden=True
	DrawType=DT_None
	bStasis=False
	bAlwaysTick=True
	bCollideActors=False
	bCollideWorld=False
	Physics=PHYS_None
}
