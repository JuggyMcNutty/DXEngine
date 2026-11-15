
#include "Precomp.h"
#include "HXGame.h"
#include "UHX.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Packages/Core/UState.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Engine/Actors/Pawn/UPawn.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/UViewport.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Extension/Flags/UFlagBase.h"
#include "VM/ScriptCall.h"
#include "Collision/BottomLevel/TraceRayModel.h"
#include "Utils/StrTools.h"
#include "Utils/Logger.h"

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

void UHXSteve::Init(const std::map<std::string, std::string>& travelInfo, const std::string& portal)
{
	if (const std::string* flags = FindTravel(travelInfo, FlagsKey))
		InitFlagBase(*flags);
	else
		LogMessage("Initializing Steve.FlagBase");
	if (const std::string* points = FindTravel(travelInfo, SkillPointsKey))
		Value<int>(Offsets().SkillPointsTotal) = StrTools::ue1_atoi(*points);
	if (const std::string* goals = FindTravel(travelInfo, GoalsKey))
		InitGoals(*goals);
	if (const std::string* keys = FindTravel(travelInfo, NanoKeysKey))
		InitNanoKeys(*keys);
	if (const std::string* images = FindTravel(travelInfo, ImagesKey))
		InitImages(*images);
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

UHXSteve* HXGameEngine::SpawnSteve(ULevel* level, ULevelInfo* levelInfo, const std::map<std::string, std::string>& travelInfo, const std::string& portal)
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
