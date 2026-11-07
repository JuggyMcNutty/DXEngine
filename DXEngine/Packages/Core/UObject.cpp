
#include "Precomp.h"
#include "UObject.h"
#include "UClass.h"
#include "Properties/UProperty.h"
#include "Properties/UBoolProperty.h"
#include "Properties/UObjectProperty.h"
#include "Properties/UStringProperty.h"
#include "Properties/UStrProperty.h"
#include "Package/Package.h"
#include "Package/PackageManager.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "VM/Bytecode.h"
#include "VM/NativeFunc.h"
#include "Engine.h"
#include "Utils/Exception.h"
#include "Utils/AlignedAlloc.h"

UObject::UObject(NameString name, UClass* cls, ObjectFlags flags) : Name(name), Class(cls), Flags(flags)
{
}

UObject::~UObject()
{
}

void UObject::LoadNow()
{
	if (DelayLoad)
	{
		auto info = std::move(DelayLoad);
		auto stream = info->package->OpenObjectStream(info->Index, info->ObjName, info->Class);
		if (!stream->IsEmptyStream())
		{
			Load(stream.get());
			// One its export gives no outer is in its package, as the
			// original's: a level and its actors, a texture in no group. A
			// class keeps none, its block being its defaults.
			UObject* outer = info->Outer;
			if (!outer && !UObject::TryCast<UClass>(this))
				outer = info->package->GetPackageObject();
			SetObject("Outer", outer);
		}
		else if (auto s = UObject::TryCast<UStruct>(this))
		{
			if (s->BaseStruct)
			{
				if (s->BaseStruct->Properties.empty())
					s->BaseStruct->LoadNow();
				s->Properties = s->BaseStruct->Properties;
				s->StructSize = s->BaseStruct->StructSize;
			}

			if (auto c = UObject::TryCast<UClass>(this))
			{
				PropertyData.Init(c);
				SetObject("Outer", info->Outer);
				SetObject("Class", Class);
				SetName("Name", Name);
				SetInt("ObjectFlags", (int)Flags);
			}
		}
	}
}

void UObject::Load(ObjectStream* stream)
{
	if (Class)
		Class->LoadNow();

	if (AllFlags(Flags, ObjectFlags::HasStack))
	{
		UStruct* func = stream->ReadObject<UStruct>();
		UState* state = stream->ReadObject<UState>();
		int64_t probeMask = stream->ReadInt64();
		int32_t latentAction = stream->ReadInt32();

		int offset = -1;
		if (func)
			offset = stream->ReadIndex();

		// An object in no state has its class for both, as the original's
		// state frame starts (UObject::InitExecution).
		if (UObject::TryCast<UClass>(func) || UObject::TryCast<UClass>(state))
		{
			func = nullptr;
			state = nullptr;
		}

		if (func && state)
		{
			if (offset != -1)
			{
				func->LoadNow();
				int index = func->Code->FindStatementIndex(offset);
				if (index != -1)
				{
					StateFrame = std::make_shared<Frame>(this, nullptr);
					StateFrame->SetState(func);
					StateFrame->State = state;
					StateFrame->StatementIndex = index;
					if (latentAction >= 0 && (size_t)latentAction < NativeFunctions::LatentActionByIndex.size())
					{
						StateFrame->LatentState = NativeFunctions::LatentActionByIndex[latentAction];
					}
					else
					{
						LogMessage("UObject::Load encountered out of bounds latent action index");
						StateFrame->LatentState = LatentRunState::Continue;
					}
				}
				else
				{
					LogMessage("UObject::Load could not find statement index");
				}
			}
			else
			{
				state->LoadNow();
				StateFrame = std::make_shared<Frame>(this, nullptr);
				StateFrame->SetState(state);
				StateFrame->LatentState = LatentRunState::Stop;
			}
		}
		else if (state)
		{
			LogMessage("UObject::Load encountered object with a state object but no function");
		}

		// The enabled probes, as the original's FStateFrame keeps them.
		CurrentProbeMask = (uint64_t)probeMask;
	}

	if (!UObject::TryCast<UClass>(this))
	{
		// The class's defaults, then the original's constructor, which runs
		// at a load too, then what the save holds over them
		PropertyData.Init(Class);
		InitNativeDefaults();
		PropertyData.Load(stream);
		if (Class && !Class->Properties.empty())
		{
			SetObject("Class", Class);
			SetName("Name", Name);
			SetInt("ObjectFlags", (int)Flags);
		}
	}
}

void UObject::Save(PackageStreamWriter* stream)
{
	if (AllFlags(Flags, ObjectFlags::HasStack))
	{
		// As the original's state frame is written: the state whose code runs
		// (in this engine's VM a StateFrame is only ever suspended in state
		// code, StateFrame->Func), then the state the object is in (its
		// State: the one GotoState found, which the node may be a super of)
		// -- its functions are the ones the original calls. An
		// object in no state has its class for both, as the original's frame
		// starts: the original reads that state's name at its next GotoState.
		UStruct* func = StateFrame ? StateFrame->Func : nullptr;
		UState* state = nullptr;
		if (func)
		{
			state = UObject::TryCast<UState>(StateFrame->State);
			if (!state)
				state = UObject::TryCast<UState>(func);
		}
		else
		{
			func = Class;
			state = Class;
		}
		int32_t latentAction = 0;
		int offset = -1;

		// The enabled probes, as the original's FStateFrame writes them.
		int64_t probeMask = (int64_t)CurrentProbeMask;

		// A StateFrame can outlive its state: GotoState("") (what a bTriggerOnceOnly mover does
		// once it has fired) keeps the frame but sets Func to null, while LatentState is left at
		// whatever it was - and defaults to Continue, never Stop. Func must be checked, or this
		// dereferences null; Func->Code can likewise be null for a state with no bytecode. The
		// first writes the class above, the second no code offset: Load restores either dormant.
		if (StateFrame && StateFrame->Func && StateFrame->LatentState != LatentRunState::Stop)
		{
			auto it = NativeFunctions::IndexForLatentAction.find(StateFrame->LatentState);
			latentAction = (it != NativeFunctions::IndexForLatentAction.end()) ? it->second : 0;
			if (StateFrame->Func->Code)
				offset = StateFrame->Func->Code->FindOffset((int)StateFrame->StatementIndex);
		}

		stream->WriteObject(func);
		stream->WriteObject(state);
		stream->WriteInt64(probeMask);
		stream->WriteInt32(latentAction);
		if (func)
			stream->WriteIndex(offset);
	}

	if (!UObject::TryCast<UClass>(this))
	{
		PropertyData.Save(stream, Class ? &Class->PropertyData : nullptr);
	}
}

PropertyDataOffset UObject::GetPropertyDataOffset(const NameString& name) const
{
	// The class's own first, as the original's FindField (Core.dll
	// 0x10143fd0): PlayerPawn's bSinglePlayer, not Actor's
	if (UProperty* prop = PropertyData.Class->FindProperty(name))
		return prop->DataOffset;
	return PropertyDataOffset();
}

UProperty* UObject::GetMemberProperty(const NameString& propName) const
{
	for (UProperty* prop : PropertyData.Class->Properties)
	{
		if (prop->Name == propName)
			return prop;
	}
	Exception::Throw("Object Property '" + Name.ToString() + "." + propName.ToString() + "' not found");
}

std::string UObject::GetPathName()
{
	// Its outers up to its package's object (Package::GetPackageObject),
	// which heads the path: a texture in a group reads
	// Effects.Laser.LaserBeam1. One whose outers end short of it, a class,
	// has its package's name put first.
	std::string path = Name.ToString();
	UObject* outermost = this;
	for (UObject* outer = Outer(); outer; outer = outer->Outer())
	{
		path = outer->Name.ToString() + "." + path;
		outermost = outer;
	}
	if (outermost->package && !outermost->package->IsPackageObject(outermost))
		path = outermost->package->GetPackageName().ToString() + "." + path;
	return path;
}

void* UObject::GetProperty(UProperty* prop)
{
	return PropertyData.Ptr(prop);
}

const void* UObject::GetProperty(UProperty* prop) const
{
	return PropertyData.Ptr(prop);
}

const void* UObject::GetProperty(const NameString& propName) const
{
	return GetProperty(GetMemberProperty(propName));
}

void* UObject::GetProperty(const NameString& propName)
{
	return GetProperty(GetMemberProperty(propName));
}

bool UObject::HasProperty(const NameString& name) const
{
	for (UProperty* prop : PropertyData.Class->Properties)
	{
		if (prop->Name == name)
			return true;
	}
	return false;
}

std::string UObject::GetPropertyAsString(const NameString& propName) const
{
	UProperty* prop = GetMemberProperty(propName);
	return prop->PrintValue(PropertyData.Ptr(prop));
}

std::string UObject::ExportPropertyText(UProperty* prop)
{
	void* data = PropertyData.Ptr(prop);
	if (UObject::TryCast<UStrProperty>(prop) || UObject::TryCast<UStringProperty>(prop))
		return ScriptToUTF8(*static_cast<ScriptString*>(data));
	if (UObject::TryCast<UObjectProperty>(prop))
	{
		UObject* value = *static_cast<UObject**>(data);
		return value ? value->Class->Name.ToString() + "'" + value->GetPathName() + "'" : "None";
	}
	return prop->PrintValue(data);
}

void UObject::SetPropertyFromString(const NameString& name, const std::string& value)
{
	UProperty* prop = GetMemberProperty(name);
	prop->SetValueFromString(PropertyData.Ptr(prop), value);
}

void UObject::SaveConfig()
{
	// Saves the instance properties the ini file
	Class->SaveProperties(&PropertyData);
}

uint8_t UObject::GetByte(const NameString& name) const
{
	return *static_cast<const uint8_t*>(GetProperty(name));
}

uint32_t UObject::GetInt(const NameString& name) const
{
	return *static_cast<const uint32_t*>(GetProperty(name));
}

bool UObject::GetBool(const NameString& propName) const
{
	UProperty* prop = GetMemberProperty(propName);
	return static_cast<UBoolProperty*>(prop)->GetBool(PropertyData.Ptr(prop));
}

float UObject::GetFloat(const NameString& name) const
{
	return *static_cast<const float*>(GetProperty(name));
}

vec3 UObject::GetVector(const NameString& name) const
{
	return *static_cast<const vec3*>(GetProperty(name));
}

Rotator UObject::GetRotator(const NameString& name) const
{
	return *static_cast<const Rotator*>(GetProperty(name));
}

const ScriptString& UObject::GetString(const NameString& name) const
{
	return *static_cast<const ScriptString*>(GetProperty(name));
}

const NameString& UObject::GetName(const NameString& name) const
{
	return *static_cast<const NameString*>(GetProperty(name));
}

UObject* UObject::GetUObject(const NameString& name)
{
	return *static_cast<UObject**>(GetProperty(name));
}

Color UObject::GetColor(const NameString& name)
{
	return *static_cast<Color*>(GetProperty(name));
}

NameString UObject::GetUClassName(UObject* obj)
{
	return obj->Class ? obj->Class->Name : NameString();
}

NameString UObject::GetUClassFullName(UObject* obj)
{
	return obj->Class ? NameString(obj->Class->package->GetPackageName().ToString() + "." + obj->Class->Name.ToString()) : NameString();
}

void UObject::SetByte(const NameString& name, uint8_t value)
{
	*static_cast<uint8_t*>(GetProperty(name)) = value;
}

void UObject::SetInt(const NameString& name, uint32_t value)
{
	*static_cast<int32_t*>(GetProperty(name)) = value;
}

void UObject::SetBool(const NameString& name, bool value)
{
	UProperty* prop = GetMemberProperty(name);
	static_cast<UBoolProperty*>(prop)->SetBool(PropertyData.Ptr(prop), value);
}

void UObject::SetFloat(const NameString& name, float value)
{
	*static_cast<float*>(GetProperty(name)) = value;
}

void UObject::SetVector(const NameString& name, const vec3& value)
{
	*static_cast<vec3*>(GetProperty(name)) = value;
}

void UObject::SetRotator(const NameString& name, const Rotator& value)
{
	*static_cast<Rotator*>(GetProperty(name)) = value;
}

void UObject::SetString(const NameString& name, const ScriptString& value)
{
	*static_cast<ScriptString*>(GetProperty(name)) = value;
}

void UObject::SetName(const NameString& name, const NameString& value)
{
	*static_cast<NameString*>(GetProperty(name)) = value;
}

void UObject::SetObject(const NameString& name, const UObject* value)
{
	*static_cast<const UObject**>(GetProperty(name)) = value;
}

bool UObject::IsA(const NameString& className) const
{
	UStruct* cls = Class;
	while (cls)
	{
		if (cls->Name == className)
			return true;
		cls = cls->BaseStruct;
	}
	return false;
}

bool UObject::IsA(const UObject* cls) const
{
	for (const UStruct* c = Class; c; c = c->BaseStruct)
	{
		if (c == cls)
			return true;
	}
	return cls == nullptr;
}

bool UObject::IsDefaultsObject() const
{
	return Class && Class->DefaultsObject == this;
}

bool UObject::IsEventEnabled(const NameString& name) const
{
	// An event of a probe, or a script call of a function named after one,
	// runs only with its bit set; any other call runs, nothing checked.
	EventName eventName = {};
	if (NameStringToEventName(name, eventName))
		return IsEventEnabled(eventName);
	return true;
}

// The probes the state or the class has a function for, less those the
// state ignores.
uint64_t UObject::ComputeProbeMask() const
{
	uint64_t mask = Class ? Class->ProbeMask : ~0ULL;
	if (StateFrame && StateFrame->State)
	{
		UState* state = static_cast<UState*>(StateFrame->State);
		mask = (mask | state->ProbeMask) & state->IgnoreMask;
	}
	return mask;
}

void UObject::EnableEvent(const NameString& name)
{
	// Sets the bit, if the state or the class has a function for the probe
	// and the state does not ignore it. A name that is not a probe is only
	// logged, as the original's.
	EventName eventName = {};
	if (!NameStringToEventName(name, eventName) || (int)eventName >= 64)
	{
		LogMessage("Enable: '" + name.ToString() + "' is not a probe function");
		return;
	}
	CurrentProbeMask |= ComputeProbeMask() & (1ULL << (int)eventName);
}

void UObject::DisableEvent(const NameString& name)
{
	// Clears the probe's bit until the mask is next set (GotoState).
	EventName eventName = {};
	if (!NameStringToEventName(name, eventName) || (int)eventName >= 64)
	{
		LogMessage("Disable: '" + name.ToString() + "' is not a probe function");
		return;
	}
	CurrentProbeMask &= ~(1ULL << (int)eventName);
}

std::string UObject::PrintProperties()
{
	std::string result;

	for (UProperty* prop : PropertyData.Class->Properties)
	{
		result += prop->Name.ToString();
		result += " = ";
		void* ptr = PropertyData.Ptr(prop);
		result += prop->PrintValue(ptr);
		result += "\n";
	}

	return result;
}

Array<UProperty*> UObject::GetAllProperties()
{
	Array<UProperty*> result;

	for (UProperty* prop : PropertyData.Class->Properties)
	{
		result.push_back(prop);
	}

	return result;
}

Array<UProperty*> UObject::GetAllUserEditableProperties()
{
	Array<UProperty*> result;

	for (UProperty* prop : PropertyData.Class->Properties)
	{
		if (bool(prop->PropFlags & PropertyFlags::Edit))
			result.push_back(prop);
	}

	return result;
}

Array<UProperty*> UObject::GetAllTravelProperties()
{
	// Only DeusEx marks Inventory as a travel property.
	//
	// In the original's order, its field iterator's (Engine.dll 0x10392830,
	// which its travel export at 0x10391740 walks): the class's own
	// properties first, then each super's in turn, where Properties holds
	// the supers' first. So the export meets what the player's own classes
	// name -- DeusExPlayer's KeyRing and inHand, Pawn's SelectedItem --
	// before Actor's Inventory chain, and the arrival, giving the items
	// their TravelPreAccept last to first, puts the chain's items back in
	// their belt slots first: an item off the chain then goes to the belt's
	// first free slot, not to one a chain item takes back from it.

	Array<UProperty*> result;
	for (UStruct* s = PropertyData.Class; s; s = s->BaseStruct)
	{
		for (UField* field = s->Children; field; field = field->Next)
		{
			UProperty* prop = UObject::TryCast<UProperty>(field);
			if (prop && (AllFlags(prop->PropFlags, PropertyFlags::Travel) || prop->Name == "Inventory"))
				result.push_back(prop);
		}
	}
	return result;
}

NameString UObject::GetStateName() const
{
	return StateFrame && StateFrame->State ? StateFrame->State->Name : NameString();
}

int UObject::GotoState(NameString stateName, const NameString& labelName, bool* labelFound)
{
	if (stateName == "Auto")
	{
		bool foundState = false;
		for (UClass* cls = Class; cls != nullptr && !foundState; cls = static_cast<UClass*>(cls->BaseStruct))
		{
			for (auto& it : cls->States)
			{
				UState* state = it.second;
				if ((state->StateFlags & ScriptStateFlags::Auto) == ScriptStateFlags::Auto)
				{
					stateName = state->Name;
					foundState = true;
					break;
				}
			}
		}
	}

	UState* newState = nullptr;

	if (!stateName.IsNone())
	{
		for (UClass* cls = Class; cls != nullptr; cls = static_cast<UClass*>(cls->BaseStruct))
		{
			UState* state = cls->GetState(stateName);
			if (state)
			{
				newState = state;
				break;
			}
		}
	}

	if (!StateFrame)
	{
		if (!TakesStateFrame())
			return 0;
		StateFrame = std::make_shared<Frame>(this, nullptr);
	}

	UState* oldState = (UState*)StateFrame->State;

	// As the original's UObject::GotoState (Core.dll 0x1012e8f0): EndState
	// under InEndState, so a GotoState from within it sends no EndState
	// again, and a state change during it (StateChanged, set by a GotoState
	// that ends in a state) preempts the rest -- the object stays where that
	// GotoState took it, this one's state and label forgotten; a change
	// during BeginState likewise keeps that state and skips this label. A
	// GotoState that ends in no state, to None or to a name the class has
	// not, does not set it: from within EndState it preempts nothing, and
	// the outer one goes on into its state. It is cleared only before an
	// EndState or a BeginState whose probe is on (IsProbing): a GotoState
	// to no state from a state with no EndState leaves it as it was, so one
	// made in BeginState after another that ended in a state still preempts.
	if (oldState && oldState != newState && IsEventEnabled(EventName::EndState) && !AnyFlags(Flags, ObjectFlags::InEndState))
	{
		Flags = (Flags & ~ObjectFlags::StateChanged) | ObjectFlags::InEndState;
		CallEvent(this, EventName::EndState);
		Flags &= ~ObjectFlags::InEndState;
		if (AnyFlags(Flags, ObjectFlags::StateChanged))
			return 2;
	}

	if (oldState != newState)
		StateFrame->SetState(newState);

	// The mask is set at every GotoState, even into the state the object
	// is in: the probes the state or the class has a function for, less
	// those the state ignores. A script's GotoState into its own state
	// goes to the label alone (NObject::GotoState).
	CurrentProbeMask = ComputeProbeMask();

	if (newState)
	{
		// HasStack is otherwise only ever inherited from the original package's export table at
		// load time. An actor that starts a level with no baked-in stack but enters a real state
		// during play would otherwise never have that state written by Save() at all, silently
		// losing it on the next save regardless of what state it's actually in.
		Flags |= ObjectFlags::HasStack;
		if (oldState != newState && IsEventEnabled(EventName::BeginState))
		{
			Flags &= ~ObjectFlags::StateChanged;
			CallEvent(this, EventName::BeginState);
			if (AnyFlags(Flags, ObjectFlags::StateChanged))
				return 2;
		}
		bool found = StateFrame->GotoLabel(labelName);
		if (labelFound)
			*labelFound = found;
		Flags |= ObjectFlags::StateChanged;
		return 1;
	}
	return 0;
}

void UObject::Mark(GCMarker& marker)
{
	marker.SetField("Class");
	marker.MarkCode(Class);
	// A package reached only through its objects is a shell: its file and
	// tables, none of its other objects (Package::Mark).
	marker.SetField("package");
	marker.MarkConst(package);
	if (DelayLoad)
	{
		marker.SetField("DelayLoad");
		marker.MarkCode(DelayLoad->Class);
		marker.MarkConst(DelayLoad->package);
		marker.Mark(DelayLoad->Outer);
	}
	if (StateFrame)
	{
		marker.SetField("StateFrame");
		StateFrame->Mark(marker);
	}
	// The values are laid out by the block's class: a class's own block is
	// its defaults.
	if (PropertyData.Data && PropertyData.Class)
	{
		for (UProperty* prop : PropertyData.Class->RefProps())
			prop->MarkProperty(marker, PropertyData.Ptr(prop));
	}
}

void UObject::MarkNames()
{
	Name.Mark();
	if (DelayLoad)
		DelayLoad->ObjName.Mark();
	if (PropertyData.Data && PropertyData.Class)
	{
		for (UProperty* prop : PropertyData.Class->NameProps())
			prop->MarkPropertyNames(PropertyData.Ptr(prop));
	}
	if (StateFrame)
		StateFrame->MarkNames();
}

void UObject::OnGCDestroy()
{
	PropertyData.Reset();
	StateFrame.reset();
	DelayLoad.reset();
	if (package && package->IsExportObject(this, (int)exportIndex))
		package->ExportObjects[exportIndex] = nullptr;
	if (package && NameCounted)
		package->ForgetObjectName(Name);
}

std::string UObject::GCClassName() const
{
	return Class ? Class->Name.ToString() : std::string("Object");
}

std::string UObject::GCDescribe() const
{
	return GCClassName() + " " + Name.ToString();
}

/////////////////////////////////////////////////////////////////////////////

void PropertyDataBlock::Reset()
{
	if (IsView)
	{
		Data = nullptr;
		Class = nullptr;
		IsView = false;
		return;
	}
	// Classes are never collected (UField::IsGCRoot), so the class that
	// laid the values out is there to destruct them.
	if (Data && Class)
	{
		for (UProperty* prop : Class->DestructProps())
			prop->DestructArray(Ptr(prop));
	}
	AlignedFree(Data);
	Data = nullptr;
	Class = nullptr;
}

void PropertyDataBlock::Init(UClass* cls)
{
	Reset();

	bool isDefaultBlock = (&cls->PropertyData == this);

	Class = cls;
	Size = cls->StructSize;
	Data = AlignedAlloc(cls->StructAlignment, cls->StructSize);

	for (UProperty* prop : cls->Properties)
	{
#ifdef _DEBUG
		if (prop->DataOffset.DataOffset + prop->ArraySize() > cls->StructSize)
			Exception::Throw("Memory corruption detected!");
#endif

		if (!isDefaultBlock)
		{
			// This is an object instance.
			// Copy all properties over from the class default block.
			prop->CopyConstructArray(Ptr(prop), cls->PropertyData.Ptr(prop));
		}
		else
		{
			// This is a default block.
			// If the property comes from the base class, copy its value over from its default block.
			// Otherwise this is a new property for this class, which we default (zero) initialize.
			if (cls->BaseStruct && prop->DataOffset.DataOffset < cls->BaseStruct->PropertyData.Size)
				prop->CopyConstructArray(Ptr(prop), cls->BaseStruct->PropertyData.Ptr(prop));
			else
				prop->ConstructArray(Ptr(prop));
		}
	}
}

void PropertyDataBlock::Load(ObjectStream* stream)
{
	while (true)
	{
		NameString name = stream->ReadName();
		if (name == "None")
			break;

		uint8_t info = stream->ReadInt8();
		bool infoBit = info & 0x80;

		PropertyHeader header;
		header.type = (UnrealPropertyType)(info & 0x0f);

		if (header.type == UPT_Struct)
			header.structName = stream->ReadName();

		switch ((info & 0x70) >> 4)
		{
		default:
		case 0: header.size = 1; break;
		case 1: header.size = 2; break;
		case 2: header.size = 4; break;
		case 3: header.size = 12; break;
		case 4: header.size = 16; break;
		case 5: header.size = stream->ReadUInt8(); break;
		case 6: header.size = stream->ReadUInt16(); break;
		case 7: header.size = stream->ReadUInt32(); break;
		}

		header.arrayIndex = 0;
		if (infoBit && header.type != UPT_Bool)
		{
			int byte1 = stream->ReadUInt8();
			if ((byte1 & 0xc0) == 0xc0)
			{
				byte1 &= 0x3f;
				int byte2 = stream->ReadUInt8();
				int byte3 = stream->ReadUInt8();
				int byte4 = stream->ReadUInt8();
				header.arrayIndex = (byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4;
			}
			else if (byte1 & 0x80)
			{
				byte1 &= 0x7f;
				int byte2 = stream->ReadUInt8();
				header.arrayIndex = (byte1 << 8) | byte2;
			}
			else
			{
				header.arrayIndex = byte1;
			}
		}
		else if (header.type == UPT_Bool)
		{
			header.boolValue = infoBit;
		}

		// The property the tag names, the class's own before its supers': the
		// original's loader walks the class's property chain
		// (SerializeTaggedProperties, Core.dll 0x10126730), which lists a
		// class's own properties before its super's (UStruct::Link,
		// 0x10126060), as FindField walks them (0x10143fd0). So a tag of a
		// property a class declares again meets the class's own, of whatever
		// type: a GameReplicationInfo's tag of Actor's Region, a struct,
		// meets its own int Region and is refused as another type.
		UProperty* prop = Class->FindProperty(name);

		// A tag the property cannot take is logged and its bytes skipped, the
		// object loading on, as that loader does, in its order and words: no
		// property of the name, another type (but a String tag of a Str, read
		// as one), an element past its static array, another struct, a native
		// or transient property (every file a persistent archive), then
		// "Skipping %i bytes of type %i"; XLevel's tag, a native one old maps
		// hold, without either of the last two lines. DXEngine threw, failing
		// the whole package or save.
		std::string owner = Class->Name.ToString();
		bool xlevel = name == "XLevel";
		PropertyHeader own = {};
		if (prop)
			prop->SaveHeader(Ptr(prop), own);
		bool refused = true;
		if (!prop)
			LogMessage("Property " + name.ToString() + " of Class " + Class->GetPathName() + " not found");
		else if (header.type != own.type && !(header.type == UPT_String && own.type == UPT_Str))
			LogMessage("Type mismatch in " + name.ToString() + " of " + owner + ": file " + std::to_string((int)header.type) + ", class " + std::to_string((int)own.type));
		else if (header.arrayIndex < 0 || header.arrayIndex >= prop->ArrayDimension)
			LogMessage("Array bounds in " + name.ToString() + " of " + owner + ": " + std::to_string(header.arrayIndex) + "/" + std::to_string(prop->ArrayDimension));
		else if (header.type == UPT_Struct && header.structName != own.structName)
			LogMessage("Property " + name.ToString() + " of " + owner + " struct type mismatch " + header.structName.ToString() + "/" + own.structName.ToString());
		else if (AnyFlags(prop->PropFlags, PropertyFlags::Native | PropertyFlags::Transient))
		{
			if (!xlevel)
				LogMessage("Property " + name.ToString() + " of " + owner + " is not serialiable");
		}
		else
			refused = false;
		if (refused)
		{
			if (!xlevel)
				LogMessage("Skipping " + std::to_string(header.size) + " bytes of type " + std::to_string((int)header.type));
			if (header.type != UPT_Bool)
				stream->Skip(header.size);
			continue;
		}

		void* data = Ptr(prop);
		prop->Flags |= ObjectFlags::TagExp;
		uint32_t start = stream->Tell();
		try
		{
			prop->LoadValue(static_cast<uint8_t*>(data) + header.arrayIndex * prop->ElementPitch(), stream, header);
		}
		catch (const std::exception& e)
		{
			LogMessage(std::string(e.what()) + " in " + name.ToString() + " of " + owner + ": skipping " + std::to_string(header.size) + " bytes of type " + std::to_string((int)header.type));
			stream->Seek(header.type != UPT_Bool ? start + header.size : start);
		}
	}
}

void PropertyDataBlock::Save(PackageStreamWriter* stream, PropertyDataBlock* defaultBlock)
{
	for (UProperty* prop : Class->Properties)
	{
		// Native properties are saved by the objects themselves
		// Transient properties don't get saved
		if (AnyFlags(prop->PropFlags, PropertyFlags::Native | PropertyFlags::Transient))
			continue;

		for (int arrayIndex = 0; arrayIndex < prop->ArrayDimension; arrayIndex++)
		{
			size_t offset = prop->ElementPitch() * arrayIndex;
			void* data = static_cast<uint8_t*>(Ptr(prop)) + offset;

			// Written only where the property's compare takes it apart from
			// the class's default, as the original's SerializeTaggedProperties
			// (Core.dll 0x10126730) by Identical: a string apart from it in
			// case alone is not, and loads as the default
			if (defaultBlock)
			{
				void* defaultdata = static_cast<uint8_t*>(defaultBlock->Ptr(prop)) + offset;
				if (prop->CompareElement(data, defaultdata))
					continue;
			}

			PropertyHeader header = {};
			header.arrayIndex = arrayIndex;
			prop->SaveHeader(data, header);
			if (header.type == UPT_Invalid)
			{
				LogMessage("Skipping saving invalid property " + prop->Name.ToString());
				continue;
			}

			stream->BeginProperty(prop->Name);
			prop->SaveValue(data, stream);
			stream->EndProperty(header);
		}
	}

	stream->WriteName(NameString());
}

/////////////////////////////////////////////////////////////////////////////

ScriptArray::ScriptArray(UProperty* type) : Type(type)
{
}

ScriptArray::ScriptArray(const ScriptArray& other) : Type(other.Type)
{
	Reserve(other.Size);
	try
	{
		auto pitch = Type->ElementPitch();
		for (size_t i = 0, count = other.Size; i < count; i++)
		{
			Type->CopyConstructElement(Data + Size * pitch, other.Data + i * pitch);
			Size++;
		}
	}
	catch (...)
	{
		Clear();
		AlignedFree(Data);
		throw;
	}
}

ScriptArray::ScriptArray(ScriptArray&& other) : Type(other.Type)
{
	Data = other.Data;
	Size = other.Size;
	Capacity = other.Capacity;
	other.Data = nullptr;
	other.Size = 0;
	other.Capacity = 0;
}

ScriptArray::~ScriptArray()
{
	if (Type)
	{
		auto pitch = Type->ElementPitch();
		while (Size > 0)
		{
			Size--;
			Type->DestructElement(Data + pitch * Size);
		}
	}
	AlignedFree(Data);
	Data = nullptr;
}

ScriptArray& ScriptArray::operator=(ScriptArray&& other) noexcept
{
	if (&other != this)
	{
		Clear();
		ShrinkToFit();
		Data = other.Data;
		Size = other.Size;
		Capacity = other.Capacity;
		other.Data = nullptr;
		other.Size = 0;
		other.Capacity = 0;
	}
	return *this;
}

void ScriptArray::ShrinkToFit()
{
	if (Size != Capacity)
	{
		ScriptArray copy(Type);
		copy.Reserve(Size);

		auto pitch = Type->ElementPitch();
		for (size_t i = 0, count = Size; i < count; i++)
		{
			Type->CopyConstructElement(copy.Data + i * pitch, Data + i * pitch);
			copy.Size++;
		}

		Swap(copy);
	}
}

void ScriptArray::Clear()
{
	auto pitch = Type->ElementPitch();
	while (Size > 0)
	{
		Type->DestructElement(Data + pitch * (Size - 1));
		Size--;
	}
}

void ScriptArray::SetValue(size_t index, const void* src)
{
	auto pitch = Type->ElementPitch();
	Type->CopyElement(Data + index * pitch, src);
}

void ScriptArray::Insert(size_t insertPos, size_t insertCount)
{
	if (insertPos == Size)
	{
		Resize(Size + insertCount);
	}
	else
	{
		size_t new_cap = Size + insertCount;
		auto pitch = Type->ElementPitch();
		ScriptArray copy(Type);
		copy.Data = (uint8_t*)AlignedAlloc(Type->ElementAlignment(), pitch * new_cap);
		if (!copy.Data) throw std::bad_alloc();
		copy.Capacity = new_cap;

		// Copy before insertion block
		for (size_t i = 0, count = insertPos; i < count; i++)
		{
			Type->CopyConstructElement(copy.Data + copy.Size * pitch, Data + i * pitch);
			copy.Size++;
		}

		// Default initialize insertion block
		for (size_t i = 0, count = insertCount; i < count; i++)
		{
			Type->ConstructElement(copy.Data + copy.Size * pitch);
			copy.Size++;
		}

		// Copy after insertion block
		for (size_t i = insertPos, count = Size; i < count; i++)
		{
			Type->CopyConstructElement(copy.Data + copy.Size * pitch, Data + i * pitch);
			copy.Size++;
		}

		Swap(copy);

	}
}

void ScriptArray::Remove(size_t removePos, size_t removeCount)
{
	auto pitch = Type->ElementPitch();
	for (size_t i = removePos, end = Size - removeCount; i < end; i++)
	{
		Type->CopyElement(Data + i * pitch, Data + (i + removeCount) * pitch);
	}
	Resize(GetSize() - removeCount);
}

void* ScriptArray::GetItem(size_t index)
{
	return Data + Type->ElementPitch() * index;
}

void ScriptArray::Reserve(size_t new_cap)
{
	if (new_cap > Capacity)
	{
		auto pitch = Type->ElementPitch();
		ScriptArray copy(Type);
		copy.Data = (uint8_t*)AlignedAlloc(Type->ElementAlignment(), pitch * new_cap);
		if (!copy.Data) throw std::bad_alloc();
		copy.Capacity = new_cap;

		for (size_t i = 0, count = Size; i < count; i++)
		{
			Type->CopyConstructElement(copy.Data + i * pitch, Data + i * pitch);
			copy.Size++;
		}

		Swap(copy);
	}
}

void ScriptArray::Swap(ScriptArray& other) noexcept
{
	std::swap(Data, other.Data);
	std::swap(Size, other.Size);
	std::swap(Capacity, other.Capacity);
}

void ScriptArray::Resize(size_t count)
{
	if (count > Size)
	{
		Reserve(count);
		auto pitch = Type->ElementPitch();
		while (Size < count)
		{
			Type->ConstructElement(Data + Size * pitch);
			Size++;
		}
	}
	else if (count < Size)
	{
		auto pitch = Type->ElementPitch();
		while (Size > count)
		{
			Type->DestructElement(Data + (Size - 1) * pitch);
			Size--;
		}
	}
}

ScriptArray& ScriptArray::operator=(const ScriptArray& other)
{
	if (&other != this)
	{
		Clear();
		Reserve(other.GetSize());
		auto pitch = Type->ElementPitch();
		for (size_t i = 0, count = other.Size; i < count; i++)
		{
			Type->CopyConstructElement(Data + Size * pitch, other.Data + i * pitch);
			Size++;
		}
	}
	return *this;
}

bool ScriptArray::operator==(const ScriptArray& rhs) const
{
	if (GetSize() != rhs.GetSize())
		return false;

	auto count = GetSize();
	auto pitch = Type->ElementPitch();
	for (size_t i = 0; i < count; i++)
	{
		uint8_t* lhsitem = Data + pitch * i;
		uint8_t* rhsitem = rhs.Data + pitch * i;
		if (!Type->CompareElement(lhsitem, rhsitem))
			return false;
	}
	return true;
}

bool ScriptArray::operator!=(const ScriptArray& rhs) const
{
	if (GetSize() != rhs.GetSize())
		return true;
	size_t count = GetSize();
	auto pitch = Type->ElementPitch();
	for (size_t i = 0; i < count; i++)
	{
		uint8_t* lhsitem = Data + pitch * i;
		uint8_t* rhsitem = rhs.Data + pitch * i;
		if (!Type->CompareElement(lhsitem, rhsitem))
			return true;
	}
	return false;
}
