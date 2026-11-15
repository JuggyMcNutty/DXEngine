
#include "Precomp.h"
#include "HXGame.h"
#include "HXCoopStarts.h"
#include "UHX.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/UState.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Extension/Flags/UFlagBase.h"
#include "VM/ScriptCall.h"
#include "GC/GC.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Utils/StrTools.h"
#include "Utils/Logger.h"
#include "Utils/Random.h"
#include "Utils/File.h"
#include "Utils/SHA1Sum.h"
#include "OriginalCommandLine.h"

namespace
{
	// HXSteve's, HXGoal's and HXNanoKeyInfo's properties, found in HX.u's
	// classes the first time they are asked for
	struct SteveOffsets
	{
		PropertyDataOffset FlagBase, SkillPointsTotal, FirstGoal, KeyList, Portal, DataVaultImages;
		PropertyDataOffset GoalName, GoalText, GoalPrimary, GoalCompleted, GoalNext;
		PropertyDataOffset KeyID, KeyDescription, KeyNext;
	};

	const SteveOffsets& Offsets()
	{
		static SteveOffsets offsets;
		static bool initialized = false;
		if (!initialized)
		{
			Package* hx = engine->packages->GetPackage("HX");
			auto offsetOf = [&](const char* className, const char* property) {
				UClass* cls = hx ? hx->GetClass(className) : nullptr;
				return cls ? cls->GetPropertyDataOffset(property) : PropertyDataOffset();
			};
			offsets.FlagBase = offsetOf("HXSteve", "FlagBase");
			offsets.SkillPointsTotal = offsetOf("HXSteve", "SkillPointsTotal");
			offsets.FirstGoal = offsetOf("HXSteve", "FirstGoal");
			offsets.KeyList = offsetOf("HXSteve", "KeyList");
			offsets.Portal = offsetOf("HXSteve", "Portal");
			offsets.DataVaultImages = offsetOf("HXSteve", "DataVaultImages");
			offsets.GoalName = offsetOf("HXGoal", "GoalName");
			offsets.GoalText = offsetOf("HXGoal", "Text");
			offsets.GoalPrimary = offsetOf("HXGoal", "bPrimaryGoal");
			offsets.GoalCompleted = offsetOf("HXGoal", "bCompleted");
			offsets.GoalNext = offsetOf("HXGoal", "Next");
			offsets.KeyID = offsetOf("HXNanoKeyInfo", "KeyID");
			offsets.KeyDescription = offsetOf("HXNanoKeyInfo", "Description");
			offsets.KeyNext = offsetOf("HXNanoKeyInfo", "NextKey");
			initialized = true;
		}
		return offsets;
	}

	const char* const FlagsKey = "__INTERNAL_Steve_Flags";
	const char* const SkillPointsKey = "__INTERNAL_Steve_SkillPointsTotal";
	const char* const GoalsKey = "__INTERNAL_Steve_Goals";
	const char* const NanoKeysKey = "__INTERNAL_Steve_NanoKeys";
	const char* const ImagesKey = "__INTERNAL_Steve_Images";

	// The travel info's key, without regard to case, as HX's TMap finds it
	const std::string* FindTravel(const std::map<std::string, std::string>& travelInfo, const char* key)
	{
		for (const auto& entry : travelInfo)
		{
			if (StrTools::equals_ignore_case(entry.first, key))
				return &entry.second;
		}
		return nullptr;
	}

	void SetTravel(std::map<std::string, std::string>& travelInfo, const char* key, const std::string& value)
	{
		for (auto it = travelInfo.begin(); it != travelInfo.end();)
		{
			if (StrTools::equals_ignore_case(it->first, key))
				it = travelInfo.erase(it);
			else
				++it;
		}
		travelInfo[key] = value;
	}

	// The text's lines, as ParseLine with Exact takes them: to each CR or LF,
	// the line breaks passed over
	Array<std::string> Lines(const std::string& text)
	{
		Array<std::string> lines;
		size_t pos = 0;
		while (pos < text.size())
		{
			size_t end = text.find_first_of("\r\n", pos);
			if (end == std::string::npos)
				end = text.size();
			lines.push_back(text.substr(pos, end - pos));
			pos = end;
			while (pos < text.size() && (text[pos] == '\r' || text[pos] == '\n'))
				pos++;
		}
		return lines;
	}

	bool ParseInt(const std::string& line, const char* key, int& value)
	{
		std::string text;
		if (!StrTools::ue1_parse(line, key, text))
			return false;
		value = StrTools::ue1_atoi(text);
		return true;
	}

	UObject* NewTransient(const char* className)
	{
		UClass* cls = engine->packages->FindClass(className);
		if (!cls)
			return nullptr;
		Package* transient = engine->packages->GetTransientPackage();
		return transient->NewObject(transient->MakeUniqueObjectName(cls), cls, ObjectFlags::NoFlags);
	}
}

/////////////////////////////////////////////////////////////////////////////

// Each key the travel info holds read, in this order; with no travel info,
// the flag base's reader given nothing, which logs its line alone
// (HX.dll 0x10022c00)
void UHXSteve::Init(const std::map<std::string, std::string>* travelInfo, const std::string& portal)
{
	if (travelInfo)
	{
		if (const std::string* flags = FindTravel(*travelInfo, FlagsKey))
			InitFlagBase(*flags);
		if (const std::string* points = FindTravel(*travelInfo, SkillPointsKey))
			Value<int>(Offsets().SkillPointsTotal) = StrTools::ue1_atoi(*points);
		if (const std::string* goals = FindTravel(*travelInfo, GoalsKey))
			InitGoals(*goals);
		if (const std::string* keys = FindTravel(*travelInfo, NanoKeysKey))
			InitNanoKeys(*keys);
		if (const std::string* images = FindTravel(*travelInfo, ImagesKey))
			InitImages(*images);
	}
	else
	{
		InitFlagBase(std::string());
	}
	Value<ScriptString>(Offsets().Portal) = UTF8ToScript(portal);
}

// HX.dll 0x10023150: each line a bool flag set, added, with its expiration;
// another type is warned about and left out
void UHXSteve::InitFlagBase(const std::string& text)
{
	LogMessage("Initializing Steve.FlagBase");
	UFlagBase* flagBase = UObject::TryCast<UFlagBase>(Value<UObject*>(Offsets().FlagBase));
	for (const std::string& line : Lines(text))
	{
		std::string name, value;
		int type = 0, expiration = 0;
		if (!StrTools::ue1_parse(line, "Name=", name, 64) || !ParseInt(line, "Type=", type) || !ParseInt(line, "Expiration=", expiration) || !StrTools::ue1_parse(line, "Value=", value, 1024))
		{
			LogMessage("Found malformed Flag line in TravelInfo.");
			continue;
		}
		if (type == 0)
		{
			if (flagBase)
				flagBase->SetBool(NameString(name), StrTools::ue1_atoi(value) != 0, true, expiration);
		}
		else if (type > 0 && type <= 6)
		{
			LogMessage("Unhandled Flag: Name=" + name + " Type=" + std::to_string(type) + " Expiration=" + std::to_string(expiration) + " Value=" + value);
		}
	}
}

// HX.dll 0x100234f0: each line a new HXGoal at the list's end
void UHXSteve::InitGoals(const std::string& text)
{
	const SteveOffsets& o = Offsets();
	for (const std::string& line : Lines(text))
	{
		std::string name, goalText;
		int primary = 0, completed = 0;
		if (!StrTools::ue1_parse(line, "GoalName=", name, 64) || !ParseInt(line, "bPrimaryGoal=", primary) || !ParseInt(line, "bCompleted=", completed) || !StrTools::ue1_parse(line, "Text=", goalText, 1024))
		{
			LogMessage("Found malformed goal line in TravelInfo.");
			continue;
		}
		UObject* goal = NewTransient("HX.HXGoal");
		if (!goal)
			continue;
		goal->Value<NameString>(o.GoalName) = NameString(name);
		goal->Value<ScriptString>(o.GoalText) = UTF8ToScript(goalText);
		goal->BoolValue(o.GoalPrimary) = (primary & 1) != 0;
		goal->BoolValue(o.GoalCompleted) = (completed & 1) != 0;
		goal->Value<UObject*>(o.GoalNext) = nullptr;
		UObject*& first = Value<UObject*>(o.FirstGoal);
		if (!first)
		{
			first = goal;
			continue;
		}
		UObject* last = first;
		while (last->Value<UObject*>(o.GoalNext))
			last = last->Value<UObject*>(o.GoalNext);
		last->Value<UObject*>(o.GoalNext) = goal;
	}
}

// HX.dll 0x100238f0: each line a new HXNanoKeyInfo at the list's end
void UHXSteve::InitNanoKeys(const std::string& text)
{
	const SteveOffsets& o = Offsets();
	for (const std::string& line : Lines(text))
	{
		std::string id, description;
		if (!StrTools::ue1_parse(line, "KeyID=", id, 64) || !StrTools::ue1_parse(line, "Description=", description, 1024))
		{
			LogMessage("Found malformed NanoKey line in TravelInfo.");
			continue;
		}
		UObject* key = NewTransient("HX.HXNanoKeyInfo");
		if (!key)
			continue;
		key->Value<NameString>(o.KeyID) = NameString(id);
		key->Value<ScriptString>(o.KeyDescription) = UTF8ToScript(description);
		key->Value<UObject*>(o.KeyNext) = nullptr;
		UObject*& first = Value<UObject*>(o.KeyList);
		if (!first)
		{
			first = key;
			continue;
		}
		UObject* last = first;
		while (last->Value<UObject*>(o.KeyNext))
			last = last->Value<UObject*>(o.KeyNext);
		last->Value<UObject*>(o.KeyNext) = key;
	}
}

// HX.dll 0x10023b10: each line's class into the next of the 63 slots
void UHXSteve::InitImages(const std::string& text)
{
	UClass** images = &Value<UClass*>(Offsets().DataVaultImages);
	int slot = 0;
	for (const std::string& line : Lines(text))
	{
		std::string path;
		if (!StrTools::ue1_parse(line, "ImageClass=", path, 1024))
		{
			LogMessage("Found malformed Images line in TravelInfo.");
			continue;
		}
		if (slot >= 63)
			break;
		UClass* cls = engine->packages->FindClass(NameString(path));
		if (!cls)
		{
			LogMessage("Failed to ImageClass for DataVault: " + path);
			continue;
		}
		images[slot++] = cls;
	}
}

// HX.dll 0x10023cb0: the five keys written, each a line a flag, goal, key or
// image, CR LF ended, as HX's string output writes them
void UHXSteve::ExportTravel(std::map<std::string, std::string>& travelInfo)
{
	const SteveOffsets& o = Offsets();
	if (UFlagBase* flagBase = UObject::TryCast<UFlagBase>(Value<UObject*>(o.FlagBase)))
	{
		std::string flags;
		int iterator = flagBase->CreateIterator((uint8_t)EFlagType::Bool);
		NameString name;
		uint8_t type = 0;
		while (flagBase->GetNextFlag(iterator, name, type))
		{
			flags += "Name=" + name.ToString() + " Type=" + std::to_string((int)type) + " Expiration=" + std::to_string(flagBase->GetExpiration(name, type)) +
				" Value=" + std::to_string(flagBase->GetBool(name) ? 1 : 0) + "\r\n";
		}
		flagBase->DestroyIterator(iterator);
		SetTravel(travelInfo, FlagsKey, flags);
	}

	SetTravel(travelInfo, SkillPointsKey, std::to_string(Value<int>(o.SkillPointsTotal)) + "\r\n");

	std::string goals;
	for (UObject* goal = Value<UObject*>(o.FirstGoal); goal; goal = goal->Value<UObject*>(o.GoalNext))
	{
		goals += "GoalName=" + goal->Value<NameString>(o.GoalName).ToString() +
			" bPrimaryGoal=" + std::to_string(goal->BoolValue(o.GoalPrimary) ? 1 : 0) +
			" bCompleted=" + std::to_string(goal->BoolValue(o.GoalCompleted) ? 1 : 0) +
			" Text=\"" + ScriptToUTF8(goal->Value<ScriptString>(o.GoalText)) + "\"\r\n";
	}
	SetTravel(travelInfo, GoalsKey, goals);

	std::string keys;
	for (UObject* key = Value<UObject*>(o.KeyList); key; key = key->Value<UObject*>(o.KeyNext))
		keys += "KeyID=" + key->Value<NameString>(o.KeyID).ToString() + " Description=\"" + ScriptToUTF8(key->Value<ScriptString>(o.KeyDescription)) + "\"\r\n";
	SetTravel(travelInfo, NanoKeysKey, keys);

	std::string images;
	UClass** slots = &Value<UClass*>(o.DataVaultImages);
	for (int i = 0; i < 63; i++)
	{
		if (slots[i])
			images += "ImageClass=" + slots[i]->GetPathName() + "\r\n";
	}
	SetTravel(travelInfo, ImagesKey, images);
}

/////////////////////////////////////////////////////////////////////////////

bool HXGameEngine::Active()
{
	static int active = -1;
	if (active < 0)
		active = StrTools::equals_ignore_case(engine->packages->GetIniValue("system", "Engine.Engine", "GameEngine", "Engine.GameEngine"), "HX.HXGameEngine") ? 1 : 0;
	return active == 1;
}

void HXGameEngine::Start(std::string& url, std::string& fallbackMap, bool dedicated)
{
	PackageManager* packages = engine->packages.get();
	if (url.empty())
		url = OriginalCommandLine::Get().FirstToken();
	fallbackMap = dedicated ? packages->GetIniValue("system", "URL", "Map", "Index.dx") : packages->GetIniValue("system", "URL", "LocalMap", "DX.dx");

	// The save folder, one level made
	std::error_code ec;
	std::filesystem::create_directory(packages->GetSaveFolderPath(), ec);

	// The game's packages, each by its GUID, five warnings for one that is
	// not the game's (CheckPackageGuid, HX.dll 0x10045bc0)
	static const char* const guids[][2] =
	{
		{ "Core", "BFC9A4E444E3B749B688A99529BD75C1" }, { "Engine", "51EA2E914B9530035CB94784FF411102" },
		{ "Editor", "C0C8923B46FC57C4F4E11AAE9AFDFE3B" }, { "Fire", "F6281FED4974E8179BA5C5A6C8F75983" },
		{ "IpDrv", "3D673AB846D927DB40979C80AEA2659B" }, { "UWindow", "EEC33ECC4F995DDDB367518CD376A8F8" },
		{ "UBrowser", "B6057DA84D35EB2FE331EC98D12183F3" }, { "Extension", "F6BC1EDF4C4EDA21F54AED822CC9CD81" },
		{ "DeusExUI", "6B9B3C64466601CBA3F1EA8D0B96354D" }, { "ConSys", "08722AAF4FACDB1C0AA14587F66F9D9A" },
		{ "DeusExConversations", "315A906E11D43C6160004CBD0AB81608" }, { "DeusExSounds", "9E3B43F7450FE7F81934ADAF750B84FD" },
		{ "DeusExItems", "B4E7F4094532B8F5E0C9EFB53D74C9BC" }, { "DeusExDeco", "315A907111D43C6160004CBD0AB81608" },
		{ "DeusExCharacters", "315A907211D43C6160004CBD0AB81608" }, { "DeusExConText", "1B71548E46F9331A675211AEB304D25F" },
		{ "DeusExConAudioAIBarks", "315A906D11D43C6160004CBD0AB81608" }, { "MPCharacters", "55738DF94887DC5BC2E36F8E85954811" },
		{ "DeusExText", "32A8AAE940701B1B09969CA278BBECD6" }, { "IpServer", "71AABCC5455A63B81C3979AFC39AD1BE" },
		{ "DeusEx", "CADECA6B4EB3B7FF1F0CB59221ECCC8E" },
	};
	for (const auto& entry : guids)
	{
		std::string guid = packages->GetPackage(entry[0])->GetGuidString();
		if (StrTools::equals_ignore_case(guid, entry[1]))
			continue;
		LogMessage(std::string(entry[0]) + " Package Guid: " + guid + " -- Bad");
		LogMessage(std::string("You are using a non standard ") + entry[0] + " package.");
		LogMessage("You will likely encounter errors and crashes.");
		LogMessage(std::string("Don't report any bugs when using this ") + entry[0] + " package.");
		LogMessage("If this is non intentional use the DeusEx 1112fm version instead.");
	}

	// A client's player: one named Player, or none, or -RANDOMDENTON, a new
	// name of one to four letters, two the likeliest, each A to Y, and
	// _Denton
	if (dedicated)
		return;
	std::string name = packages->GetIniValue("user", "DefaultPlayer", "Name");
	if (!name.empty() && !StrTools::equals_ignore_case(name, "Player") && !OriginalCommandLine::Get().Param("RANDOMDENTON"))
		return;
	double u = FRand();
	double letters = u < 1.0 / 3.0 ? 1.0 + std::sqrt(3.0 * u) : 4.0 - std::sqrt(6.0 * (1.0 - u));
	int count = std::clamp((int)std::nearbyint(letters), 1, 5);
	std::string generated;
	for (int i = 0; i < count; i++)
		generated += (char)std::clamp((int)std::nearbyint(FRand() * 25.0 - 0.5) + 'A', (int)'A', (int)'Z');
	generated += "_Denton";
	packages->SetIniValue("user", "DefaultPlayer", "Name", generated);
}

// HX's LoadMap: bSpawning True on each actor whose class has the bool (its
// public property, as ImportText sets it, HX.dll 0x100421d0)
void HXGameEngine::SetSpawning(ULevel* level)
{
	NameString spawning("bSpawning");
	for (UActor* actor : level->Actors)
	{
		if (!actor)
			continue;
		if (!actor->HasProperty(spawning) || !UObject::TryCast<UBoolProperty>(actor->GetMemberProperty(spawning)))
			continue;
		actor->BoolValue(actor->GetPropertyDataOffset(spawning)) = true;
	}
}

// The player's own HX.dll, read once: the System folder's, where the
// original's HX.exe loads it from, else the one beside HX.u; none, logged
// once, for one that is not HX 0.9.89.4's, whose code HXCoopStarts follows
static const Array<uint8_t>* PlayersHXDll()
{
	static bool tried = false;
	static Array<uint8_t> bytes;
	static bool found = false;
	if (tried)
		return found ? &bytes : nullptr;
	tried = true;

	PackageManager* packages = engine->packages.get();
	Array<fs::path> candidates = { packages->GetSystemFolderPath() / "HX.dll" };
	std::string hxPackage = packages->FindPackageFile(NameString("HX"), {});
	if (!hxPackage.empty())
		candidates.push_back(fs::path(hxPackage).parent_path() / "HX.dll");
	for (const fs::path& candidate : candidates)
	{
		fs::path path = Directory::resolveCase(candidate);
		std::error_code ec;
		if (!fs::is_regular_file(path, ec))
			continue;
		std::string sha1 = SHA1Sum::of_file(path);
		if (!StrTools::equals_ignore_case(sha1, HXCoopStarts::DllSha1))
		{
			LogMessage("HX.dll is not HX 0.9.89.4's (" + path.string() + ", SHA1 " + sha1 + "): no co-op starts");
			return nullptr;
		}
		bytes = File::read_all_bytes(path.string());
		found = true;
		return &bytes;
	}
	LogMessage("HX.dll not found: no co-op starts");
	return nullptr;
}

void HXGameEngine::SpawnCoopStarts(ULevelInfo* levelInfo, const std::string& map)
{
	// HX's asserts the class's load
	UClass* cls = engine->packages->FindClass("HX.HXCoopStart");
	if (!cls)
	{
		LogMessage("HX.HXCoopStart not found: no co-op starts");
		return;
	}
	const Array<uint8_t>* dll = PlayersHXDll();
	if (!dll)
		return;
	Array<HXCoopStarts::Start> starts;
	std::string error;
	if (!HXCoopStarts::Read(*dll, map, starts, error))
	{
		LogMessage("HX.dll's co-op starts not read: " + error);
		return;
	}

	PropertyDataOffset priority = cls->GetPropertyDataOffset("Priority");
	PropertyDataOffset defaultStart = cls->GetPropertyDataOffset("bDefaultStart");
	PropertyDataOffset teamNumber = cls->GetPropertyDataOffset("TeamNumber");
	PropertyDataOffset initiallyEnabled = cls->GetPropertyDataOffset("bInitiallyEnabled");
	PropertyDataOffset enableTag = cls->GetPropertyDataOffset("EnableTag");
	PropertyDataOffset disableTag = cls->GetPropertyDataOffset("DisableTag");
	PropertyDataOffset enabledMessage = cls->GetPropertyDataOffset("EnabledMessage");
	for (const HXCoopStarts::Start& start : starts)
	{
		// HXSpawnSingleStart (HX.dll 0x10025cb0): the level's SpawnActor with
		// no name, owner or instigator, a spawn that may fail
		UActor* actor = levelInfo->Spawn(cls, nullptr, std::nullopt, start.location, start.rotation, false, false);
		if (!actor)
		{
			char text[512];
			std::snprintf(text, sizeof(text), "Failed to spawn coop start at (%f,%f,%f) with Tag '%s'", start.location.x, start.location.y, start.location.z, start.tag.c_str());
			LogMessage(text);
			continue;
		}
		actor->Tag() = NameString(start.tag);
		actor->Value<int32_t>(priority) = start.priority;
		actor->BoolValue(defaultStart) = start.defaultStart;
		actor->Value<uint8_t>(teamNumber) = start.teamNumber;
		actor->BoolValue(initiallyEnabled) = start.initiallyEnabled;
		actor->Value<NameString>(enableTag) = NameString(start.enableTag);
		actor->Value<NameString>(disableTag) = NameString(start.disableTag);
		actor->Value<ScriptString>(enabledMessage) = UTF8ToScript(start.enabledMessage);
		actor->SetCollision(start.collideActors, false, false);
	}
}

static bool IsA(UActor* actor, const char* className)
{
	NameString name(className);
	for (UClass* cls = actor->Class; cls; cls = static_cast<UClass*>(cls->BaseStruct))
	{
		if (cls->Name == name)
			return true;
	}
	return false;
}

void HXGameEngine::SetupActors(ULevel* level)
{
	struct MoverSwap { UClass* from; UClass* to; };
	PackageManager* packages = engine->packages.get();
	MoverSwap swaps[] =
	{
		{ packages->FindClass("DeusEx.DeusExMover"), packages->FindClass("HX.HXMover") },
		{ packages->FindClass("DeusEx.BreakableGlass"), packages->FindClass("HX.HXBreakableGlass") },
		{ packages->FindClass("DeusEx.BreakableWall"), packages->FindClass("HX.HXBreakableWall") },
	};

	for (UActor* actor : level->Actors)
	{
		if (!actor)
			continue;
		if (IsA(actor, "Decoration") || IsA(actor, "Effects") || IsA(actor, "Projectile") || IsA(actor, "Triggers"))
		{
			if (actor->LightType() != 0)
			{
				std::string name = actor->Name.ToString();
				if (actor->bStatic() && actor->bNoDelete())
					LogMessage("Keeping " + name + " as bStatic and bNoDelete due to LightType!=LT_None.");
				else if (actor->bStatic())
					LogMessage("Keeping " + name + " as bStatic due to LightType!=LT_None.");
				else if (actor->bNoDelete())
					LogMessage("Keeping " + name + " as bNoDelete due to LightType!=LT_None.");
			}
			else
			{
				actor->bStatic() = false;
				actor->bNoDelete() = false;
			}
			actor->bGameRelevant() = false;
		}
		else if (IsA(actor, "Info"))
		{
			if (IsA(actor, "DeusExLevelInfo"))
			{
				actor->RemoteRole() = ROLE_None;
				actor->bNoDelete() = true;
			}
			else if (IsA(actor, "ZoneInfo"))
			{
				actor->bGameRelevant() = false;
			}
		}
		else if (IsA(actor, "Keypoint"))
		{
			if (IsA(actor, "CameraPoint"))
			{
				actor->bNoDelete() = true;
				actor->RemoteRole() = ROLE_None;
				UState* idle = nullptr;
				for (UClass* cls = actor->Class; cls && !idle; cls = static_cast<UClass*>(cls->BaseStruct))
					idle = cls->GetState("Idle");
				if (idle)
					idle->StateFlags = {};
				else
					LogMessage("Failed to set state CameraPoint.Idle to 0");
			}
			else if (IsA(actor, "InterpolationPoint"))
			{
				actor->bNoDelete() = true;
				actor->RemoteRole() = ROLE_None;
				if (UFunction* beginPlay = FindEventFunction(actor, NameString("BeginPlay")))
					beginPlay->FuncFlags = beginPlay->FuncFlags | FunctionFlags::Simulated;
			}
		}
		else if (actor->Brush() && IsA(actor, "Brush"))
		{
			// The movers made HX's in place: HX's classes declare the game's
			// own variables, in their order
			for (const MoverSwap& swap : swaps)
			{
				if (swap.from && swap.to && actor->Class == swap.from && swap.to->StructSize == swap.from->StructSize)
				{
					actor->Class = swap.to;
					break;
				}
			}
		}
	}
}

UHXSteve* HXGameEngine::SpawnSteve(ULevel* level, ULevelInfo* levelInfo, const std::map<std::string, std::string>* travelInfo, const std::string& portal)
{
	UClass* steveClass = engine->packages->FindClass("HX.HXSteve");
	if (!steveClass)
		return nullptr;
	Package* levelPackage = engine->LevelPackage;
	UHXSteve* steve = UObject::TryCast<UHXSteve>(levelPackage->NewObject(levelPackage->MakeUniqueObjectName(steveClass), steveClass, ObjectFlags::Transactional | ObjectFlags::LoadContextFlags | ObjectFlags::HasStack));
	if (!steve)
		return nullptr;
	steve->XLevel() = level;
	steve->Level() = levelInfo;
	level->Collision.AddToCollision(steve);
	steve->Tag() = steveClass->Name;
	steve->bTicked() = false;
	steve->InitActorZone();
	steve->Index = (int)level->Actors.size();
	level->Actors.push_back(steve);
	level->ActorsVersion++;

	// A new HXFlagBase, in the transient package, then the travel's state
	steve->Value<UObject*>(Offsets().FlagBase) = NewTransient("HX.HXFlagBase");
	steve->Init(travelInfo, portal);
	return steve;
}

void HXGameEngine::ExportSteves(ULevel* level, std::map<std::string, std::string>& travelInfo)
{
	LogMessage("Adding Steve to TravelInfo.");
	bool first = true;
	for (UActor* actor : level->Actors)
	{
		UHXSteve* steve = UObject::TryCast<UHXSteve>(actor);
		if (!steve)
			continue;
		if (!first)
			LogMessage("!!! WARNING !!! Found multiple Steve's");
		first = false;
		steve->ExportTravel(travelInfo);
	}
}

// The order UE1's own sort leaves (the Sort of its templates, which HX.dll
// carries at 0x1004c4b0): a run of up to 8 sorted by moving its largest to
// its end, again and again; a longer one parted round its middle element,
// moved to its front first, the larger part kept on a stack and the smaller
// sorted next
template<typename T, typename Compare>
static void UE1Sort(T* first, int num, Compare compare)
{
	if (num < 2)
		return;
	struct Range { T* min; T* max; };
	Range stack[32];
	int top = 0;
	stack[0] = { first, first + num - 1 };
	while (top >= 0)
	{
		Range cur = stack[top--];
		for (;;)
		{
			int count = (int)(cur.max - cur.min + 1);
			if (count <= 8)
			{
				while (cur.max > cur.min)
				{
					T* largest = cur.min;
					for (T* item = cur.min + 1; item <= cur.max; item++)
					{
						if (compare(*item, *largest) > 0)
							largest = item;
					}
					std::swap(*largest, *cur.max);
					cur.max--;
				}
				break;
			}
			std::swap(cur.min[count / 2], cur.min[0]);
			T* lo = cur.min;
			T* hi = cur.max + 1;
			for (;;)
			{
				while (++lo <= cur.max && compare(*lo, *cur.min) <= 0) {}
				while (--hi > cur.min && compare(*hi, *cur.min) >= 0) {}
				if (lo > hi)
					break;
				std::swap(*lo, *hi);
			}
			std::swap(*cur.min, *hi);
			if (hi - 1 - cur.min >= cur.max - lo)
			{
				if (cur.min + 1 < hi)
					stack[++top] = { cur.min, hi - 1 };
				if (cur.max > lo)
				{
					cur.min = lo;
					continue;
				}
			}
			else
			{
				if (cur.max > lo)
					stack[++top] = { lo, cur.max };
				if (cur.min + 1 < hi)
				{
					cur.max = hi - 1;
					continue;
				}
			}
			break;
		}
	}
}

// A distance as HX's x87 code takes it: the squares' sum held at the FPU's
// precision, its root stored as a float
static double SquaredDistance(const vec3& a, const vec3& b)
{
	double dx = (double)a.x - b.x;
	double dy = (double)a.y - b.y;
	double dz = (double)a.z - b.z;
	return dx * dx + dy * dy + dz * dz;
}

void HXGameEngine::SetDistances(ULevel* level)
{
	ULevelInfo* info = level->Actors.empty() ? nullptr : UObject::TryCast<ULevelInfo>(level->Actors[0]);
	if (!info)
		return;
	uint8_t mode = info->NetMode();
	if (mode == NM_Client)
	{
		UPlayerPawn* player = engine->viewport ? engine->viewport->Actor() : nullptr;
		if (!player || !player->Player())
			return;
		for (size_t i = level->FirstDynamicActor; i < level->Actors.size(); i++)
		{
			if (UActor* actor = level->Actors[i])
				actor->DistanceFromPlayer() = (float)std::sqrt(SquaredDistance(actor->Location(), player->Location()));
		}
		return;
	}
	if (mode != NM_Standalone && mode != NM_ListenServer && mode != NM_DedicatedServer)
		return;

	Array<UPawn*> players;
	for (UPawn* pawn = info->PawnList(); pawn; pawn = pawn->nextPawn())
	{
		if (pawn->bIsPlayer() && UObject::TryCast<UHXPlayerPawn>(pawn))
			players.push_back(pawn);
	}

	// An HX NPC's PlayerDistances: 32 of HX's PlayerDistance, a player and
	// its distance
	struct Slot { UObject* player; float distance; };
	for (size_t i = level->FirstDynamicActor; i < level->Actors.size(); i++)
	{
		UActor* actor = level->Actors[i];
		if (!actor)
			continue;
		if (!actor->bIsPawn() || !UObject::TryCast<UHXScriptedPawn>(actor))
		{
			float best = 1.0e20f;
			for (UPawn* player : players)
			{
				double d2 = SquaredDistance(actor->Location(), player->Location());
				if (d2 < best)
					best = (float)d2;
			}
			actor->DistanceFromPlayer() = (float)std::sqrt((double)best);
			continue;
		}

		UStructProperty* property = UObject::TryCast<UStructProperty>(actor->GetMemberProperty(NameString("PlayerDistances")));
		if (!property || !property->Struct)
			continue;
		size_t playerOffset = ~(size_t)0, distanceOffset = ~(size_t)0;
		for (UProperty* member : property->Struct->Properties)
		{
			if (member->Name == NameString("Player"))
				playerOffset = member->DataOffset.DataOffset;
			else if (member->Name == NameString("Distance"))
				distanceOffset = member->DataOffset.DataOffset;
		}
		if (playerOffset == ~(size_t)0 || distanceOffset == ~(size_t)0)
			continue;

		int count = (int)std::min<size_t>(players.size(), 32);
		Slot slots[32] = {};
		for (int n = 0; n < count; n++)
			slots[n] = { players[n], (float)std::sqrt(SquaredDistance(actor->Location(), players[n]->Location())) };
		UE1Sort(slots, count, [](const Slot& a, const Slot& b) { return b.distance >= a.distance ? -1 : 1; });

		uint8_t* data = static_cast<uint8_t*>(actor->GetProperty(property));
		for (int n = 0; n < 32; n++)
		{
			uint8_t* element = static_cast<uint8_t*>(property->GetElement(data, n));
			*reinterpret_cast<UObject**>(element + playerOffset) = slots[n].player;
			*reinterpret_cast<float*>(element + distanceOffset) = slots[n].distance;
		}
		actor->DistanceFromPlayer() = count > 0 ? slots[0].distance : 1.0e10f;
	}
}

bool HXGameEngine::FastRepLineCheck(ULevel* level, const vec3& end, const vec3& start)
{
	if (!level)
		return true;
	CollisionSystem& collision = level->Collision;
	return TraceRayModel::FastWalkSeeThrough(collision.LineNodes(), collision.LineRootOutside(),
		[&](int surf) { return collision.LineSurface(surf).PolyFlags; }, start, end);
}

// An object property of the object's, made None, where its class has it
static void ClearProperty(UObject* obj, const char* name)
{
	NameString property(name);
	if (!obj->HasProperty(property))
		return;
	UObjectProperty* prop = UObject::TryCast<UObjectProperty>(obj->GetMemberProperty(property));
	if (!prop)
		return;
	void* data = obj->GetProperty(prop);
	for (int i = 0; i < prop->ArrayDimension; i++)
		*static_cast<UObject**>(prop->GetElement(data, i)) = nullptr;
}

static bool ObjectIsA(UObject* obj, const char* className)
{
	NameString name(className);
	for (UClass* cls = obj->Class; cls; cls = static_cast<UClass*>(cls->BaseStruct))
	{
		if (cls->Name == name)
			return true;
	}
	return false;
}

void HXGameEngine::ExitLevelCleanup()
{
	for (GCObject* gcobj : GC::GetObjects(false))
	{
		UObject* obj = dynamic_cast<UObject*>(gcobj);
		if (!obj || !obj->Class || (obj->Flags & ObjectFlags::EliminateObject) == ObjectFlags::EliminateObject)
			continue;
		if (ObjectIsA(obj, "ConCamera"))
		{
			for (const char* name : { "cameraActor", "conLightSpeaker", "conLightSpeakingTo", "firstActor", "secondActor", "lastFirstActor", "lastSecondActor" })
				ClearProperty(obj, name);
			obj->Flags |= ObjectFlags::EliminateObject;
		}
		else if (ObjectIsA(obj, "ConEventAnimation") || ObjectIsA(obj, "ConEventTrade"))
		{
			ClearProperty(obj, "eventOwner");
		}
		else if (ObjectIsA(obj, "ConEventMoveCamera"))
		{
			ClearProperty(obj, "cameraActor");
		}
		else if (ObjectIsA(obj, "ConEventSpeech"))
		{
			ClearProperty(obj, "speaker");
			ClearProperty(obj, "speakingTo");
		}
		else if (ObjectIsA(obj, "ConEventTransferObject"))
		{
			ClearProperty(obj, "fromActor");
			ClearProperty(obj, "toActor");
		}
		else if (ObjectIsA(obj, "Flag"))
		{
			ClearProperty(obj, "flagBase");
			ClearProperty(obj, "nextFlag");
			obj->Flags |= ObjectFlags::EliminateObject;
		}
		else if (ObjectIsA(obj, "FlagBase"))
		{
			ClearProperty(obj, "hashTable");
			obj->Flags |= ObjectFlags::EliminateObject;
		}
		else if (ObjectIsA(obj, "DeusExNote") || ObjectIsA(obj, "DeusExGoal") || ObjectIsA(obj, "DeusExLog"))
		{
			ClearProperty(obj, "next");
			obj->Flags |= ObjectFlags::EliminateObject;
		}
		else if (ObjectIsA(obj, "DeusExPlayer"))
		{
			for (const char* name : { "KeyList", "conHistory", "FirstGoal", "LastGoal", "FirstNote", "LastNote", "FirstLog", "LastLog" })
				ClearProperty(obj, name);
		}
		else if (ObjectIsA(obj, "HXGameInfo"))
		{
			for (const char* name : { "FirstNote", "LastNote", "FirstLog", "LastLog" })
				ClearProperty(obj, name);
		}
	}
}
