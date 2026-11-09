
#include "Precomp.h"
#include "NPlayerPawn.h"
#include "VM/NativeFunc.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/UPlayer.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Engine.h"
#include "Network/NetServer.h"
#include "Package/PackageManager.h"
#include "VM/ScriptCall.h"
#include "Utils/StrTools.h"

void NPlayerPawn::RegisterFunctions()
{
	RegisterVMNativeFunc_3("PlayerPawn", "ClientTravel", &NPlayerPawn::ClientTravel, 0);
	RegisterVMNativeFunc_2("PlayerPawn", "ConsoleCommand", &NPlayerPawn::ConsoleCommand, 0);
	RegisterVMNativeFunc_1("PlayerPawn", "CopyToClipboard", &NPlayerPawn::CopyToClipboard, 0);
	RegisterVMNativeFunc_2("PlayerPawn", "GetDefaultURL", &NPlayerPawn::GetDefaultURL, 0);
	RegisterVMNativeFunc_1("PlayerPawn", "GetEntryLevel", &NPlayerPawn::GetEntryLevel, 0);
	RegisterVMNativeFunc_1("PlayerPawn", "GetPlayerNetworkAddress", &NPlayerPawn::GetPlayerNetworkAddress, 0);
	RegisterVMNativeFunc_1("PlayerPawn", "PasteFromClipboard", &NPlayerPawn::PasteFromClipboard, 0);
	RegisterVMNativeFunc_0("PlayerPawn", "ResetKeyboard", &NPlayerPawn::ResetKeyboard, 544);
	RegisterVMNativeFunc_3("PlayerPawn", "UpdateURL", &NPlayerPawn::UpdateURL, 546);
}

void NPlayerPawn::ClientTravel(UObject* Self, const std::string& URL, uint8_t TravelType, bool bItems)
{
	// PreTravel goes to every actor from Engine::ClientTravel, as the
	// original's SetClientTravel sends it; here it went to the player alone.
	engine->ClientTravel(URL, static_cast<ETravelType>(TravelType), bItems);
}

// The original's (Engine.dll 0x103ba200): the pawn's own player runs the
// command -- the local player's viewport, a remote player's connection on a
// server --, what it wrote the answer; a pawn with no player runs nothing
// (bugs.md WIN.20, WIN.21).
void NPlayerPawn::ConsoleCommand(UObject* Self, const std::string& Command, std::string& ReturnValue)
{
	ReturnValue = engine->PawnConsoleCommand(UObject::Cast<UPlayerPawn>(Self), Command);
}

// The desktop's clipboard, by choice (NATIVES.md, the UI): the 16-bit text
// as UTF-8, both ways; a dedicated server's held by the engine, by choice
void NPlayerPawn::CopyToClipboard(UObject* Self, const ScriptString& Text)
{
	engine->SetClipboard(ScriptToUTF8(Text));
}

// As the original's (Engine.dll 0x103b8a90): a URL given the options of
// [DefaultPlayer] (Engine::DefaultPlayerOptions), and the first that begins
// Option= in any case answers, its text after that, as FURL::GetOption
// (0x103f9ee0); "" with none. So an empty Option is the line with no name's
// value, which was "".
void NPlayerPawn::GetDefaultURL(UObject* Self, const std::string& Option, std::string& ReturnValue)
{
	const std::string match = Option + "=";
	for (const std::string& option : engine->DefaultPlayerOptions())
	{
		if (option.size() >= match.size() && StrTools::equals_ignore_case(std::string_view(option).substr(0, match.size()), match))
		{
			ReturnValue = option.substr(match.size());
			return;
		}
	}
	ReturnValue.clear();
}

void NPlayerPawn::GetEntryLevel(UObject* Self, UObject*& ReturnValue)
{
	ReturnValue = engine->EntryLevelInfo;
}

// The original's (Engine.dll 0x103df310): a pawn whose player is a net
// connection -- a remote player's, on the server -- answers its address,
// a.b.c.d:port; any other "".
void NPlayerPawn::GetPlayerNetworkAddress(UObject* Self, std::string& ReturnValue)
{
	NetConnection* connection = NetConnectionOfPlayer(UObject::Cast<UPlayerPawn>(Self)->Player());
	ReturnValue = connection ? connection->RemoteAddressString() : "";
}

void NPlayerPawn::PasteFromClipboard(UObject* Self, ScriptString& ReturnValue)
{
	ReturnValue = UTF8ToScript(engine->GetClipboard());
}

void NPlayerPawn::ResetKeyboard(UObject* Self)
{
	// The original's ResetConfig of the input's class copies nothing in
	// Deus Ex and has the input read the player's bindings from User.ini
	// again (core-dll.md, configuration); DXEngine's binding store reads
	// from the same ini. DeusExPlayer.TravelPostAccept calls this on every
	// level change.
	engine->LoadKeybindings();
}

// The option into the game engine's last URL (Engine.dll
// APlayerPawn::execUpdateURL 0x103b87c0), which travel goes by.
void NPlayerPawn::UpdateURL(UObject* Self, const std::string& NewOption, const std::string& NewValue, bool bSaveDefault)
{
	engine->LastURL.AddOrReplaceOption(NewOption + "=" + NewValue);
	if (bSaveDefault)
	{
		// Save the setting to DefaultUser section in User.ini
		engine->packages->SetIniValue("User", "DefaultPlayer", NewOption, NewValue);
	}
}
