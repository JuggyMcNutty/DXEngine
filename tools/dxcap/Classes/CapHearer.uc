//=============================================================================
// CapHearer: a MessagingSpectator for WordsServer, which an actor's
// BroadcastMessage tells as it tells the players (Actor.BroadcastMessage:
// each pawn that is a player or a MessagingSpectator), so a dedicated
// server's SAY (UGameEngine::Exec, Engine.dll 0x1038a030: on a server with
// no client, the level's BroadcastMessage of the rest of the line, beeping)
// is seen on the server. Logs each message with "DXSWORDS:".
//=============================================================================
class CapHearer extends MessagingSpectator;

event ClientMessage(coerce string S, optional name Type, optional bool bBeep)
{
	Log("DXSWORDS: heard '" $ S $ "' type " $ Type $ " beep " $ bBeep);
}

defaultproperties
{
	RemoteRole=ROLE_None
	bHidden=True
	bCollideActors=False
	bCollideWorld=False
	bBlockActors=False
	bBlockPlayers=False
}
