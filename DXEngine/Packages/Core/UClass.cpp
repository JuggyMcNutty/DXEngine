
#include "Precomp.h"
#include "Properties/UArrayProperty.h"
#include "UClass.h"
#include "UEnum.h"
#include "Properties/UPointerProperty.h"
#include "Properties/UIntProperty.h"
#include "Properties/UClassProperty.h"
#include "Properties/UNameProperty.h"
#include "Properties/UStringProperty.h"
#include "Properties/UStrProperty.h"
#include "Properties/UBoolProperty.h"
#include "Properties/UByteProperty.h"
#include "Properties/UFloatProperty.h"
#include "VM/Bytecode.h"
#include "VM/NativeFunc.h"
#include "VM/ScriptCall.h"
#include "Package/PackageManager.h"
#include "Engine.h"

UClass::UClass(NameString name, UClass* base, ObjectFlags flags) : UState(std::move(name), nullptr, flags, base)
{
	if (base)
		ClsFlags = base->ClsFlags;

	if (name == "Object")
	{
		// Object is special as its the base class for everything. Described in Core.u with circular references.
		// This creates just enough of the properties to resolve it, hopefully.

		auto objInternal = GC::Alloc<UIntProperty>("ObjectInternal", nullptr, ObjectFlags::Native);
		objInternal->ArrayDimension = 6;
		Properties.push_back(objInternal);

		Properties.push_back(GC::Alloc<UObjectProperty>("Outer", nullptr, ObjectFlags::Native));
		Properties.push_back(GC::Alloc<UIntProperty>("ObjectFlags", nullptr, ObjectFlags::Native));
		Properties.push_back(GC::Alloc<UNameProperty>("Name", nullptr, ObjectFlags::Native));
		Properties.push_back(GC::Alloc<UClassProperty>("Class", nullptr, ObjectFlags::Native));

		size_t offset = 0;
		size_t structAlignment = 1;
		for (UProperty* prop : Properties)
		{
			size_t alignment = prop->ArrayAlignment();
			size_t size = prop->ArraySize();
			prop->DataOffset.DataOffset = (offset + alignment - 1) / alignment * alignment;
			offset = prop->DataOffset.DataOffset + size;
			structAlignment = std::max(structAlignment, alignment);
		}
		StructAlignment = structAlignment;
		StructSize = offset;
	}
}

void UClass::Load(ObjectStream* stream)
{
	UState::Load(stream);

	if (stream->GetVersion() <= 61)
	{
		OldClassRecordSize = stream->ReadUInt32();
		Flags = Flags | ObjectFlags::Public | ObjectFlags::Standalone;
	}

	ClsFlags = (ClassFlags)stream->ReadUInt32();
	stream->ReadBytes(ClassGuid.Data, 16);

	int NumDependencies = stream->ReadIndex();
	for (int i = 0; i < NumDependencies; i++)
	{
		ClassDependency dep;
		dep.Class = stream->ReadObject<UClass>();
		dep.Deep = stream->ReadUInt32();
		dep.ScriptTextCRC = stream->ReadUInt32();
		Dependencies.push_back(dep);
	}

	int NumPackageImports = stream->ReadIndex();
	for (int i = 0; i < NumPackageImports; i++)
		PackageImports.push_back(stream->ReadIndex());

	if (stream->GetVersion() >= 62)
	{
		ClassWithin = stream->ReadIndex();
		ClassConfigName = stream->ReadName();
	}

	PropertyData.Init(this);
	PropertyData.Load(stream);

	// Copy native UObject properties into the VM
	SetObject("Class", this);
	SetName("Name", Name);
	SetInt("ObjectFlags", (int)Flags);

	LoadProperties(&PropertyData);

	for (UField* child = Children; child; child = child->Next)
	{
		if (auto state = UObject::TryCast<UState>(child))
		{
			States[child->Name] = state;
		}
	}
}

void UClass::Save(PackageStreamWriter* stream)
{
	UState::Save(stream);

	if (stream->GetVersion() <= 61)
		stream->WriteUInt32(OldClassRecordSize);

	stream->WriteUInt32((uint32_t)ClsFlags);
	stream->WriteBytes(ClassGuid.Data, 16);

	stream->WriteIndex((int)Dependencies.size());
	for (const ClassDependency& dep : Dependencies)
	{
		stream->WriteObject(dep.Class);
		stream->WriteUInt32(dep.Deep);
		stream->WriteUInt32(dep.ScriptTextCRC);
	}

	stream->WriteIndex((int)PackageImports.size());
	for (int v : PackageImports)
		stream->WriteIndex(v);

	if (stream->GetVersion() >= 62)
	{
		stream->WriteIndex(ClassWithin);
		stream->WriteName(ClassConfigName);
	}

	PropertyData.Save(stream, nullptr);
}

UProperty* UClass::GetProperty(const NameString& propName)
{
	for (UProperty* prop : PropertyData.Class->Properties)
	{
		if (prop->Name == propName)
			return prop;
	}
	Exception::Throw("Class Property '" + Name.ToString() + "." + propName.ToString() + "' not found");
}

void UClass::SaveConfig()
{
	// Saves the default object properties to the ini file
	SaveProperties(&PropertyData);
}

void UClass::LoadProperties(PropertyDataBlock* propertyBlock)
{
	NameString sectionName = package->GetPackageName().ToString() + "." + Name.ToString();
	NameString configName = ClassConfigName;
	if (configName.IsNone()) configName = "system";
	for (UProperty* prop : Properties)
	{
		if (AnyFlags(prop->PropFlags, PropertyFlags::Config | PropertyFlags::GlobalConfig | PropertyFlags::Localized))
		{
			void* ptr = propertyBlock->Ptr(prop);
			for (int arrayIndex = 0; arrayIndex < prop->ArrayDimension; arrayIndex++)
			{
				NameString name = prop->Name;
				if (prop->ArrayDimension > 1)
					name = NameString(name.ToString() + "[" + std::to_string(arrayIndex) + "]");

				std::string value;
				bool found = false;
				NameString iniName = configName, iniSection = sectionName;
				if (AllFlags(prop->PropFlags, PropertyFlags::GlobalConfig))
				{
					if (UClass* outer = UObject::TryCast<UClass>(prop->Outer()))
					{
						iniSection = outer->package->GetPackageName().ToString() + "." + outer->Name.ToString();
						iniName = outer->ClassConfigName;
						if (iniName.IsNone()) iniName = "system";
						found = package->GetPackageManager()->FindIniValue(iniName, iniSection, name, value);
					}
				}
				else if (AllFlags(prop->PropFlags, PropertyFlags::Config))
				{
					found = package->GetPackageManager()->FindIniValue(iniName, iniSection, name, value);
				}
				else if (AllFlags(prop->PropFlags, PropertyFlags::Localized))
				{
					value = package->GetPackageManager()->Localize(package->GetPackageName(), Name, name);
				}

				if (found && value.empty())
				{
					// A key with an empty value is still a value: the original's
					// LoadConfig hands it to the property's ImportText, which
					// makes a string empty, a name, object or class None and a
					// float 0, and leaves an int, byte, bool or struct as it was
					// (dx-reverse-info/core-dll.md, configuration) -- a server's
					// empty ServerName= is its name.
					if (UObject::IsType<UStrProperty>(prop) || UObject::IsType<UStringProperty>(prop)) *static_cast<std::string*>(ptr) = std::string();
					else if (UObject::IsType<UNameProperty>(prop)) *static_cast<NameString*>(ptr) = NameString();
					else if (UObject::IsType<UFloatProperty>(prop)) *static_cast<float*>(ptr) = 0.0f;
					else if (UObject::IsType<UObjectProperty>(prop)) *static_cast<UObject**>(ptr) = nullptr;
				}
				else if (!value.empty())
				{
					// The property's own import, as the original's LoadConfig
					// and the localization hand each value to ImportText
					// (core-dll.md, configuration): a struct's members by
					// name, their quotes and nesting read, a class's or an
					// object's path, a bool's 1, True, 0 or False, an int read
					// as far as it is a number; a dynamic array from the
					// section's lines of its name. DXEngine's own parsing of
					// each type threw on the types it lacked.
					if (auto arrayprop = UObject::TryCast<UArrayProperty>(prop))
					{
						if (AnyFlags(prop->PropFlags, PropertyFlags::Config | PropertyFlags::GlobalConfig))
						{
							Array<std::string> elements = package->GetPackageManager()->GetIniValues(iniName, iniSection, name);
							ScriptArray& vec = *static_cast<ScriptArray*>(ptr);
							vec.Resize(elements.size());
							for (size_t i = 0; i < elements.size(); i++)
								arrayprop->Inner->SetValueFromString(vec.GetItem(i), elements[i]);
						}
					}
					else if (UObject::IsType<UStringProperty>(prop))
						*static_cast<std::string*>(ptr) = value;
					else
						prop->SetValueFromString(ptr, value);
				}

				ptr = static_cast<uint8_t*>(ptr) + prop->ElementPitch();
			}
		}
	}
}

// A config value's text as the original's SaveConfig writes it (Core.dll
// 0x10151540, ExportText with no flags): a string bare, an enum by its name,
// an object or class as Type'Path', a struct as its members' text
static std::string ConfigText(UProperty* prop, void* ptr)
{
	if (UObject::IsType<UStrProperty>(prop) || UObject::IsType<UStringProperty>(prop))
		return *static_cast<std::string*>(ptr);
	return prop->PrintValue(ptr);
}

void UClass::SaveProperties(PropertyDataBlock* propertyBlock)
{
	if (!(ClsFlags & ClassFlags::Config))
		return;

	NameString sectionName = package->GetPackageName().ToString() + "." + Name.ToString();
	NameString configName = ClassConfigName;
	if (configName.IsNone()) configName = "system";

	for (UProperty* prop : Properties)
	{
		if (AnyFlags(prop->PropFlags, PropertyFlags::Config | PropertyFlags::GlobalConfig))
		{
			auto ptr = propertyBlock->Ptr(prop);
			for (int arrayIndex = 0; arrayIndex < prop->ArrayDimension; arrayIndex++)
			{
				NameString name = prop->Name;
				if (prop->ArrayDimension > 1)
					name = NameString(name.ToString() + "[" + std::to_string(arrayIndex) + "]");

				NameString iniName = configName, iniSection = sectionName;
				bool write = AnyFlags(prop->PropFlags, PropertyFlags::Config);
				if (AnyFlags(prop->PropFlags, PropertyFlags::GlobalConfig))
				{
					write = false;
					if (UClass* outer = UObject::TryCast<UClass>(prop->Outer()))
					{
						iniSection = outer->package->GetPackageName().ToString() + "." + outer->Name.ToString();
						iniName = outer->ClassConfigName;
						if (iniName.IsNone()) iniName = "system";
						write = true;
					}
				}
				if (write)
				{
					// Every config type, as the original writes them: a dynamic
					// array one line of its name per element, the rest one key;
					// DXEngine left a class, object, struct or array out
					if (auto arrayprop = UObject::TryCast<UArrayProperty>(prop))
					{
						ScriptArray& vec = *static_cast<ScriptArray*>(ptr);
						Array<std::string> elements;
						for (size_t i = 0; i < vec.GetSize(); i++)
							elements.push_back(ConfigText(arrayprop->Inner, vec.GetItem(i)));
						package->GetPackageManager()->SetIniValues(iniName, iniSection, name, elements);
					}
					else
					{
						package->GetPackageManager()->SetIniValue(iniName, iniSection, name, ConfigText(prop, ptr));
					}
				}
				ptr = static_cast<uint8_t*>(ptr) + prop->ElementPitch();
			}
		}
	}

	if (propertyBlock != &PropertyData)
	{
		// If its not our own block getting saved we need to load them into our default block
		LoadProperties(&PropertyData);
	}

	// GlobalConfig properties are shared across the whole class hierarchy that
	// declares them, so a change must propagate to every class deriving from the
	// *declaring* class - not just classes deriving from `this`. Otherwise sibling
	// classes (e.g. UMenuLoadGameClientWindow vs UMenuSaveGameClientWindow, which
	// both inherit globalconfig SlotNames[] from UMenuSlotClientWindow) keep a stale
	// default until the next restart re-reads the ini.
	Array<UStruct*> propagationRoots;
	propagationRoots.push_back(this);
	for (UProperty* prop : Properties)
	{
		if (AnyFlags(prop->PropFlags, PropertyFlags::GlobalConfig))
		{
			if (UClass* declaring = UObject::TryCast<UClass>(prop->Outer()))
			{
				bool known = false;
				for (UStruct* root : propagationRoots)
					if (root == declaring) { known = true; break; }
				if (!known)
					propagationRoots.push_back(declaring);
			}
		}
	}

	for (GCObject* gcObj : GC::GetObjects())
	{
		if (UClass* cls = dynamic_cast<UClass*>(gcObj))
		{
			bool propagate = false;
			for (UStruct* base = cls; base && !propagate; base = base->BaseStruct)
			{
				for (UStruct* root : propagationRoots)
				{
					if (base == root)
					{
						propagate = true;
						break;
					}
				}
			}
			// Reload through cls's own property list (not this->Properties), so the
			// property offsets match cls's data block. this->Properties can be a
			// superset of an ancestor/sibling's layout and would overrun its block.
			if (propagate && cls != this)
				cls->LoadProperties(&cls->PropertyData);
		}
	}
}

void UClass::Mark(GCMarker& marker)
{
	UState::Mark(marker);
	marker.SetField("Dependencies");
	for (ClassDependency& dep : Dependencies)
		marker.MarkCode(dep.Class);
	marker.SetField("States");
	for (auto& it : States)
		marker.MarkCode(it.second);
}
