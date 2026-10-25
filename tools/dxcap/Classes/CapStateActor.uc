//=============================================================================
// CapStateActor: StateConsole's bouncer -- its state code goes from Ping to
// Pong and back by GotoState alone, counting each entry (Bounces), which the
// console reads each second.
//=============================================================================
class CapStateActor extends Info;

var int Bounces;

auto state Ping
{
Begin:
	Bounces++;
	GotoState('Pong');
}

state Pong
{
Begin:
	Bounces++;
	GotoState('Ping');
}
