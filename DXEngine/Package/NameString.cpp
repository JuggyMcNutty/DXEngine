
#include "Precomp.h"
#include "NameString.h"

Array<std::string> NameString::Names;
std::unordered_map<std::string, int> NameString::CompareStringToIndex;
std::unordered_map<std::string, std::pair<int, int>> NameString::SpellStringToIndex;
std::vector<uint8_t> NameString::Marks;
std::vector<int> NameString::FirstSpelled;
std::vector<int> NameString::Available;
std::vector<std::pair<int, int>> NameString::CollectableSpellings;
std::vector<std::pair<int, int>>* NameString::Recording = nullptr;

std::string NameString::Folded(const std::string& value)
{
	std::string folded = value;
	for (char& c : folded)
	{
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 'a' + 'A');
	}
	return folded;
}

// The names the original makes before any other, each in its own slot, in
// this case (FName::StaticInit, Core.dll 0x1014bbc0): the property types,
// the packages, the script's words, the probes, the log's kinds and the
// rest. None, its slot 0, is the table's first entry here too.
static const char* const HardcodedNames[] =
{
	"ByteProperty", "IntProperty", "BoolProperty", "FloatProperty", "ObjectProperty",
	"NameProperty", "StringProperty", "ClassProperty", "ArrayProperty", "StructProperty",
	"VectorProperty", "RotatorProperty", "StrProperty", "MapProperty", "FixedArrayProperty",
	"Core", "Engine", "Editor", "UnrealI", "UnrealShare", "Byte", "Int", "Bool", "Float", "Name",
	"String", "Struct", "Vector", "Rotator", "Color", "Plane", "Begin", "State", "Function",
	"Self", "True", "False", "Transient", "Enum", "Replication", "Reliable", "Unreliable",
	"Always", "Field", "Object", "TextBuffer", "Linker", "LinkerLoad", "LinkerSave", "Subsystem",
	"Factory", "TextBufferFactory", "Exporter", "StackNode", "Property", "Camera", "Spawned",
	"Destroyed", "GainedChild", "LostChild", "Probe4", "Probe5", "Trigger", "UnTrigger", "Timer",
	"HitWall", "Falling", "Landed", "ZoneChange", "Touch", "UnTouch", "Bump", "BeginState",
	"EndState", "BaseChange", "Attach", "Detach", "ActorEntered", "ActorLeaving", "KillCredit",
	"AnimEnd", "EndedRotation", "InterpolateEnd", "EncroachingOn", "EncroachedBy",
	"FootZoneChange", "HeadZoneChange", "PainTimer", "SpeechTimer", "MayFall", "Probe34", "Die",
	"Tick", "PlayerTick", "Expired", "Probe39", "SeePlayer", "EnemyNotVisible", "HearNoise",
	"UpdateEyeHeight", "SeeMonster", "SeeFriend", "SpecialHandling", "BotDesireability",
	"Probe48", "Probe49", "Probe50", "Probe51", "Probe52", "Probe53", "Probe54", "Probe55",
	"Probe56", "Probe57", "Probe58", "Probe59", "Probe60", "Probe61", "Probe62", "All", "Vect",
	"Rot", "ArrayCount", "EnumCount", "Else", "If", "Goto", "Stop", "Until", "While", "Do",
	"Break", "For", "ForEach", "Assert", "Switch", "Case", "Default", "Continue", "Private",
	"Const", "Out", "Export", "Skip", "Coerce", "Optional", "Input", "Config", "Travel",
	"EditConst", "Localized", "GlobalConfig", "SafeReplace", "New", "Expands", "Intrinsic",
	"Within", "Abstract", "Package", "Guid", "Parent", "Class", "Extends", "NoExport",
	"NoUserCreate", "PerObjectConfig", "NativeReplication", "Auto", "Ignores", "Global", "Super",
	"Outer", "Operator", "PreOperator", "PostOperator", "Final", "Iterator", "Latent", "Return",
	"Singular", "Simulated", "Exec", "Event", "Static", "Native", "Invariant", "Var", "Local",
	"Import", "From", "Spawn", "Array", "Map", "Tag", "Role", "RemoteRole", "System", "User",
	"Log", "Critical", "Init", "Exit", "Cmd", "Play", "Console", "Warning", "ExecWarning",
	"ScriptWarning", "ScriptLog", "Dev", "DevNet", "DevPath", "DevNetTraffic", "DevAudio",
	"DevLoad", "DevSave", "DevGarbage", "DevKill", "DevReplace", "DevMusic", "DevSound",
	"DevCompile", "DevBind", "Localization", "Compatibility", "NetComeGo", "Title", "Error",
	"Heading", "SubHeading", "FriendlyError", "Progress", "UserPrompt", "White", "Black", "Red",
	"Green", "Blue", "Cyan", "Magenta", "Yellow", "DefaultColor", "KeyType", "KeyEvent", "Write",
	"Message", "InitialState", "Texture", "Sound", "FireTexture", "IceTexture", "WaterTexture",
	"WaveTexture", "WetTexture", "Main", "NotifyLevelChange", "VideoChange", "SendText",
	"SendBinary", "ConnectFailure"
};

const std::string& NameString::FirstSpelling(const std::string& folded, const std::string& value)
{
	static const std::unordered_map<std::string, std::string> spellings = []()
	{
		std::unordered_map<std::string, std::string> table;
		for (const char* name : HardcodedNames)
			table[Folded(name)] = name;
		return table;
	}();
	auto it = spellings.find(folded);
	return it != spellings.end() ? it->second : value;
}

NameString NameString::Collectable(const std::string& value)
{
	NameString name;
	if (!value.empty())
		name.GetIndex(value, true);
	return name;
}

void NameString::BeginMark()
{
	for (uint8_t& mark : Marks)
		mark &= ~MarkedFlag;
}

std::vector<int> NameString::Sweep()
{
	// A spelling goes first, then its compare entry once no spelling of it
	// is left: a spelling kept keeps its compare entry.
	for (auto& spelling : CollectableSpellings)
	{
		if (Marks[spelling.first] != CollectableFlag)
			Marks[spelling.second] |= MarkedFlag;
	}

	std::vector<int> freed;
	std::vector<std::pair<int, int>> kept;
	for (auto& spelling : CollectableSpellings)
	{
		int spelled = spelling.first, compare = spelling.second;
		if (Marks[spelled] == CollectableFlag)
		{
			SpellStringToIndex.erase(Names[spelled]);
			Names[spelled] = std::string();
			Marks[spelled] = 0;
			Available.push_back(spelled);
			if (Marks[compare] == CollectableFlag)
			{
				CompareStringToIndex.erase(Names[compare]);
				Names[compare] = std::string();
				Marks[compare] = 0;
				Available.push_back(compare);
				freed.push_back(compare);
			}
		}
		else if (Marks[spelled] & CollectableFlag)
		{
			kept.push_back(spelling);
		}
	}
	CollectableSpellings = std::move(kept);
	return freed;
}
