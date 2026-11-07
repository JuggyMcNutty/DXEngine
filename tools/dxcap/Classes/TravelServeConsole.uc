//=============================================================================
// TravelServeConsole: a listen server that travels -- a deathmatch on
// DXMP_Cathedral in the package's own CapDeathMatch, as ServeConsole's, and
// once another player has been in 10 s, `servertravel DXMP_Smuggler`: the
// game sends its clients after it, and the server follows its countdown.
// Every 2 s it logs the map it is in and where each player stands; it exits
// after 150 s. With Items (DXCAP_CONFIG "Items=True"), each other player is
// first given Binoculars and the travel takes the players' items
// (Level.ServerTravel with bItems): each player's line says whether it
// holds Binoculars, so a remote player's items are seen to arrive with it,
// as the original's SpawnPlayActor takes them.
//=============================================================================
class TravelServeConsole extends Console;

var float CapTime, LogTime, InTime;
var int Phase;
var config bool Items;

function string Held(PlayerPawn Other)
{
	if (!Items)
		return "";
	if (Other.FindInventoryType(class'DeusEx.Binoculars') != None)
		return ", Binoculars held";
	return ", no Binoculars";
}

event Tick(float Delta)
{
	local PlayerPawn P, Other;
	local int Others;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;

	if (Phase == 0 && CapTime > 3.0)
	{
		Log("DXCAP: starting the listen server");
		P.ConsoleCommand("start DXMP_Cathedral?game=DXCapture.CapDeathMatch?listen");
		Phase = 1;
	}
	else if (Phase >= 1 && Phase < 9)
	{
		Others = 0;
		foreach P.AllActors(class'PlayerPawn', Other)
			if (Other != P)
				Others++;
		if (Phase == 1 && P.Level.NetMode == NM_ListenServer && Others > 0)
		{
			InTime += Delta;
			if (InTime > 10.0 && Items)
			{
				foreach P.AllActors(class'PlayerPawn', Other)
					if (Other != P)
						P.Spawn(class'DeusEx.Binoculars', Other).GiveTo(Other);
				Log("DXCAP: travelling to DXMP_Smuggler with the players' items");
				P.Level.ServerTravel("DXMP_Smuggler", True);
				Phase = 2;
			}
			else if (InTime > 10.0)
			{
				Log("DXCAP: travelling to DXMP_Smuggler");
				P.ConsoleCommand("servertravel DXMP_Smuggler");
				Phase = 2;
			}
		}
		if (CapTime - LogTime >= 2.0)
		{
			LogTime = CapTime;
			Log("DXCAP: in " $ P.Level.GetLocalURL() $ ", net mode " $ P.Level.NetMode $ ", " $ Others $ " other players");
			foreach P.AllActors(class'PlayerPawn', Other)
				Log("DXCAP: " $ string(Other) $ " at " $ Other.Location $ Held(Other));
		}
	}

	if (Phase < 9 && CapTime > 150.0)
	{
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
