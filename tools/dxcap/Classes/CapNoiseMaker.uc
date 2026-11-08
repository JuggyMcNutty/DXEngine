//=============================================================================
// CapNoiseMaker: an Info EventRingConsole spawns beside its listeners, which
// raises the CapPing pulses they hear (Ping: AISendEvent, audio, 1, 4000,
// itself the sender, so the listeners' distance to it is fixed whatever the
// player does), and at the end Vanish: it destroys itself, then, in the same
// function, raises one more pulse, logged with "DXEVTRING:". The original's
// AISendEvent and AIStartEvent give a sender pending kill a value and a
// radius of 0 (UEventManager::AISendEvent, Engine.dll 0x10382c60,
// AIStartEvent 0x10382fe0, after the actor's IsPendingKill), so no listener
// hears it, in both engines (UEventManager::SendEvent).
//=============================================================================
class CapNoiseMaker extends Info;

function Ping()
{
	AISendEvent('CapPing', EAITYPE_Audio, 1.0, 4000);
}

function Vanish()
{
	Destroy();
	AISendEvent('CapPing', EAITYPE_Audio, 1.0, 4000);
	Log("DXEVTRING: " $ Name $ " raised CapPing after its Destroy, bDeleteMe " $ bDeleteMe);
}
