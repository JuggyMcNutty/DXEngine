#pragma once

#include "UState.h"

struct Guid
{
	uint8_t Data[16] = {};
};

struct ClassDependency
{
	UClass* Class = nullptr;
	uint32_t Deep = 0;
	uint32_t ScriptTextCRC = 0;
};

enum class ClassFlags : uint32_t
{
	Abstract = 0x00001,
	Compiled = 0x00002,
	Config = 0x00004,
	Transient = 0x00008,
	Parsed = 0x00010,
	Localized = 0x00020,
	SafeReplace = 0x00040,
	RuntimeStatic = 0x00080,
	NoExport = 0x00100,
	NoUserCreate = 0x00200,
	PerObjectConfig = 0x00400,
	NativeReplication = 0x00800,
};

inline uint32_t operator&(const ClassFlags lhs, const ClassFlags rhs) { return uint32_t(lhs) & uint32_t(rhs); }
inline uint32_t operator|(const ClassFlags lhs, const ClassFlags rhs) { return uint32_t(lhs) | uint32_t(rhs); }
inline uint32_t operator^(const ClassFlags lhs, const ClassFlags rhs) { return uint32_t(lhs) ^ uint32_t(rhs); }

class UClass : public UState
{
public:
	void Mark(GCMarker& marker) override;
	// Scripts write its defaults: walked every collection (GC::Collect)
	bool IsGCMutableCode() const override { return true; }

	UClass(NameString name, UClass* base, ObjectFlags flags);

	void Load(ObjectStream* stream) override;
	void Save(PackageStreamWriter* stream) override;

	UProperty* GetProperty(const NameString& name);

	// The class's defaults as an object of its nearest native class, made
	// once, whose block is a view of the class's own (PropertyData): what
	// the property accessors read the defaults through, and a static call's
	// Self, as the original's default object is.
	template<typename T>
	T* GetDefaultObject()
	{
		UObject* obj = DefaultsObject ? DefaultsObject : MakeDefaultsObject();
		obj->PropertyData.ViewOf(PropertyData);
		return static_cast<T*>(obj);
	}
	UObject* DefaultsObject = nullptr;

	void SaveConfig() override;

	// The class's config and localized properties read into the block, from
	// its own section; with localized false its config ones alone. Every
	// section from fileOf's config file (this class's without one), as the
	// original's LoadConfig reads its object's class's file alone
	void LoadProperties(PropertyDataBlock* propertyBlock, bool localized = true, const UClass* fileOf = nullptr);
	// The config read again into the block as the original's LoadConfig with
	// its propagation reads it (Core.dll 0x10150d60): each base class's
	// section first, from the topmost config class down, then the class's
	// own, all from fileOf's config file (this class's without one)
	void LoadConfigPropagated(PropertyDataBlock* propertyBlock, const UClass* fileOf = nullptr);
	void SaveProperties(PropertyDataBlock* propertyBlock);

	uint32_t OldClassRecordSize = 0;
	ClassFlags ClsFlags = {};
	Guid ClassGuid;
	Array<ClassDependency> Dependencies;
	Array<int> PackageImports;
	int ClassWithin = 0;
	NameString ClassConfigName;

	UState* GetState(const NameString& name) { auto it = States.find(name); if (it != States.end()) return it->second; else return nullptr; }
	std::map<NameString, UState*> States;

	// FindScriptFunction's answers, for virtual calls and events: (state
	// name, function name) compare indexes to the function found, or null.
	// Classes, states and their functions do not change once loaded.
	std::unordered_map<uint64_t, UFunction*> VirtualFunctionCache;

	// The count Package::MakeUniqueObjectName names this class's objects
	// by, as the original's ClassUnique.
	int ClassUnique = 0;

private:
	UObject* MakeDefaultsObject();
};
