//=============================================================================
// CapWatcher: a UNATCOTroop SeeConsole stands in front of the player -- its
// state CapWatching, modelled on CapWader's CapMoving (reactions blocked,
// bumps and injuries ignored, out of stasis), holds it still and logs, with
// "DXSEE:", each SeePlayer and whom it names, and each EnemyNotVisible. With
// bProbeEnemy False it ignores EnemyNotVisible.
//=============================================================================
class CapWatcher extends UNATCOTroop;

var bool bProbeEnemy;

state CapWatching
{
	ignores Bump, Frob, ReactToInjury, HearNoise;

	function SeePlayer(Actor Seen)
	{
		Log("DXSEE: " $ Name $ " saw " $ Seen.Name $ " at " $ Level.TimeSeconds);
	}

	function EnemyNotVisible()
	{
		Log("DXSEE: " $ Name $ " enemy not visible at " $ Level.TimeSeconds);
	}

	function BeginState()
	{
		StandUp();
		BlockReactions(true);
		bCanConverse = False;
		SeekPawn = None;
		EnableCheckDestLoc(false);
		bStasis = False;
		if (!bProbeEnemy)
			Disable('EnemyNotVisible');
	}

	function EndState()
	{
		ResetReactions();
		bCanConverse = True;
	}

Begin:
	Acceleration = vect(0,0,0);
	Stop;
}
