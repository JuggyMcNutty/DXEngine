#include "Precomp.h"
#include "PropertyRows.h"
#include "NativeConfigClasses.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Package/IniFile.h"
#include "Package/IniProperty.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UEnum.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Packages/Core/Properties/UBoolProperty.h"
#include "Packages/Core/Properties/UByteProperty.h"
#include "Packages/Core/Properties/UStrProperty.h"
#include "Packages/Core/Properties/UObjectProperty.h"
#include "Packages/Core/Properties/UClassProperty.h"
#include "Packages/Core/Properties/UStructProperty.h"
#include "Packages/Core/Properties/UArrayProperty.h"
#include "Native/NObject.h"
#include "GC/GC.h"
#include "Utils/StrTools.h"
#include "Utils/File.h"
#include <algorithm>

/////////////////////////////////////////////////////////////////////////////
// The rows

void PropertyRow::Expand()
{
	if (Expanded || !HasBox())
		return;
	std::vector<std::unique_ptr<PropertyRow>> rows;
	MakeChildren(rows);
	Children.clear();
	for (auto& row : rows)
	{
		row->Parent = this;
		row->Depth = Depth + 1;
		Children.push_back(std::move(row));
	}
	Expanded = true;
}

void PropertyRow::Collapse()
{
	Expanded = false;
	Children.clear();
}

namespace
{
	struct OpenRows
	{
		std::string Caption;
		std::vector<OpenRows> Under;
	};

	std::vector<OpenRows> OpenOf(const PropertyRow* row)
	{
		std::vector<OpenRows> open;
		for (const auto& child : row->GetChildren())
		{
			if (child->IsExpanded())
				open.push_back({ child->GetCaption(), OpenOf(child.get()) });
		}
		return open;
	}

	void Reopen(PropertyRow* row, const std::vector<OpenRows>& open)
	{
		for (const auto& child : row->GetChildren())
		{
			for (const OpenRows& o : open)
			{
				if (o.Caption == child->GetCaption())
				{
					child->Expand();
					Reopen(child.get(), o.Under);
					break;
				}
			}
		}
	}
}

void PropertyRow::Rebuild()
{
	if (!Expanded)
		return;
	std::vector<OpenRows> open = OpenOf(this);
	Collapse();
	Expand();
	Reopen(this, open);
}

PropertyRow* PropertyRow::GetObjectRow()
{
	for (PropertyRow* row = this; row; row = row->Parent)
	{
		if (row->IsObjectRow())
			return row;
	}
	return nullptr;
}

// Rows in the order of their names, as the original's window lists a
// class's properties and the Preferences' captions: A to Z folded, each
// other byte as it is (appStricmp's order), "Joy1", "Joy10", "Joy2"
static void SortRows(std::vector<std::unique_ptr<PropertyRow>>& rows)
{
	std::stable_sort(rows.begin(), rows.end(), [](const std::unique_ptr<PropertyRow>& a, const std::unique_ptr<PropertyRow>& b) {
		return NameString::Folded(a->GetCaption()) < NameString::Folded(b->GetCaption());
	});
}

static bool Same(const std::string& a, const std::string& b)
{
	return StrTools::equals_ignore_case(a, b);
}

/////////////////////////////////////////////////////////////////////////////
// What the rows show of classes

// A class's caption as the class combos show it: its .int's ClassCaption,
// the localization's marker where there is none
// ("<?int?DXCapture.PrefsConsole.ClassCaption?>"); a language its name
static std::string LanguageCaption(const std::string& language)
{
	// Core.<language>'s [Language] Language: Core.int's "English
	// (International)"
	std::string caption;
	if (Same(language, "int"))
	{
		if (engine->packages->FindLocalized("Core", "Language", "Language", caption))
			return caption;
	}
	else
	{
		try
		{
			IniFile file((engine->packages->GetSystemFolderPath() / ("Core." + language)).string());
			if (file.FindValue("Language", "Language", caption))
				return caption;
		}
		catch (...)
		{
		}
	}
	return language;
}

static std::string ClassCaption(const std::string& path, const char* metaClass)
{
	if (path.empty() || Same(path, "None"))
		return {};
	if (metaClass && Same(metaClass, "Core.Language"))
		return LanguageCaption(path);
	size_t dot = path.find('.');
	if (dot == std::string::npos)
		return path;
	std::string package = path.substr(0, dot), name = path.substr(dot + 1);
	std::string caption;
	if (engine->packages->FindLocalized(package, name, "ClassCaption", caption))
		return caption;
	return "<?int?" + package + "." + name + ".ClassCaption?>";
}

struct ClassChoice
{
	std::string Path;
	std::string Caption;
};

// A class property's choices: the .int files' Object lines of a class of its
// kind (Class=Class, its MetaClass), each by its caption (Drivers'
// AudioDevice "Galaxy 3D Audio for Windows"); for a language, each
// Core.<language> file of System's but Core.dll, Core.u and Core.ilk, as the
// original registers a language for each (Core.dll's CacheDrivers,
// 0x10158900)
static std::vector<ClassChoice> ClassChoices(const char* metaClass)
{
	std::vector<ClassChoice> choices;
	if (!metaClass)
		return choices;
	if (Same(metaClass, "Core.Language"))
	{
		std::error_code ec;
		for (fs::directory_iterator it(engine->packages->GetSystemFolderPath(), ec), end; !ec && it != end; it.increment(ec))
		{
			std::error_code type;
			if (!it->is_regular_file(type))
				continue;
			std::string name = it->path().filename().string();
			if (name.size() <= 5 || !Same(name.substr(0, 5), "Core."))
				continue;
			std::string language = name.substr(5);
			if (Same(language, "dll") || Same(language, "u") || Same(language, "ilk"))
				continue;
			choices.push_back({ language, LanguageCaption(language) });
		}
		return choices;
	}
	for (const IntObject& obj : engine->packages->GetIntObjects(metaClass))
	{
		if (obj.Class == NameString("Class"))
			choices.push_back({ obj.Name.ToString(), ClassCaption(obj.Name.ToString(), metaClass) });
	}
	return choices;
}

static int ClassChoiceIndex(const std::vector<ClassChoice>& choices, const std::string& path)
{
	for (size_t i = 0; i < choices.size(); i++)
	{
		if (Same(choices[i].Path, path))
			return (int)i;
	}
	return -1;
}

// The class a combo's caption names: the choice's path, else the text
static std::string ClassFromCaption(const std::vector<ClassChoice>& choices, const std::string& text)
{
	for (const ClassChoice& choice : choices)
	{
		if (choice.Caption == text)
			return choice.Path;
	}
	return text;
}

static std::string Localized(const char* package, const char* section, const char* key, const char* fallback)
{
	std::string text;
	if (engine->packages->FindLocalized(package, section, key, text) && !text.empty())
		return text;
	return fallback;
}

using NativeType = NativeConfigProperty::Type;

/////////////////////////////////////////////////////////////////////////////
// A C++ class's rows (NativeConfigClasses: their values, edits and saves)

namespace
{
	// A scalar of a C++ class
	class NativeValueRow : public PropertyRow
	{
	public:
		NativeValueRow(const NativeConfigClass* cls, const NativeConfigProperty* prop) : Class(cls), Prop(prop)
		{
			Caption = prop->Name;
		}

		std::string GetValue() override
		{
			std::string text = NativeConfigValue(Class, *Prop, Prop->Name);
			if (Prop->PropType == NativeType::Class)
				return ClassCaption(text, Prop->MetaClass);
			return text;
		}

		PropertyEditor GetEditor() override
		{
			switch (Prop->PropType)
			{
			case NativeType::Bool:
			case NativeType::Enum:
			case NativeType::Class: return PropertyEditor::Combo;
			case NativeType::Byte: return PropertyEditor::Slider;
			default: return PropertyEditor::Text;
			}
		}

		std::vector<std::string> GetChoices() override
		{
			std::vector<std::string> choices;
			if (Prop->PropType == NativeType::Bool)
				choices = { "False", "True" };
			else if (Prop->PropType == NativeType::Enum)
				choices.assign(Prop->EnumNames, Prop->EnumNames + Prop->EnumCount);
			else if (Prop->PropType == NativeType::Class)
			{
				for (const ClassChoice& choice : ClassChoices(Prop->MetaClass))
					choices.push_back(choice.Caption);
			}
			return choices;
		}

		int GetChoice() override
		{
			std::string text = NativeConfigValue(Class, *Prop, Prop->Name);
			if (Prop->PropType == NativeType::Class)
				return ClassChoiceIndex(ClassChoices(Prop->MetaClass), text);
			std::vector<std::string> choices = GetChoices();
			auto it = std::find(choices.begin(), choices.end(), text);
			return it == choices.end() ? -1 : (int)(it - choices.begin());
		}

		// As the original's GlobalSetProperty (Core.dll 0x101523a0), the
		// class's object the engine keeps set as SET sets it, else the
		// class's section written whole (OBSERVED: an edit box's Enter
		// writes the section at once, the Preferences line Immediate or not)
		bool SetValue(const std::string& input) override
		{
			std::string text = input;
			if (Prop->PropType == NativeType::Class)
				text = ClassFromCaption(ClassChoices(Prop->MetaClass), input);
			SetNativeConfigValue(Class, *Prop, Prop->Name, text);
			return true;
		}

	private:
		const NativeConfigClass* Class;
		const NativeConfigProperty* Prop;
	};

	// A dynamic array of a C++ class (Core.System's Paths and Suppress, the
	// game engine's ServerActors and ServerPackages): its section's lines,
	// each change made as GlobalSetProperty makes it -- Engine.GameEngine's
	// to the running engine's list too, its line Immediate (as ruled) --
	class NativeArrayRow;

	class NativeElementRow : public PropertyRow
	{
	public:
		NativeElementRow(NativeArrayRow* array, size_t index);
		std::string GetValue() override;
		PropertyEditor GetEditor() override { return PropertyEditor::Text; }
		bool SetValue(const std::string& text) override;
		bool IsDynamicElement() override { return true; }
		void ElementInsert() override;
		void ElementDelete() override;

	private:
		NativeArrayRow* Array;
		size_t Index;
	};

	class NativeArrayRow : public PropertyRow
	{
	public:
		NativeArrayRow(const NativeConfigClass* cls, const NativeConfigProperty* prop, bool immediate) : Class(cls), Prop(prop), Immediate(immediate)
		{
			Caption = prop->Name;
		}

		bool HasBox() override { return true; }
		bool OpensByDoubleClick() override { return true; }
		std::string GetValue() override { return "..."; }
		bool IsDynamicArray() override { return true; }

		::Array<std::string> Values() { return NativeConfigArray(Class, *Prop); }
		// An element's edit saved as any edit; an array button's change the
		// defaults' in memory alone, as the original's writes nothing
		void Change(bool save, const std::function<void(::Array<std::string>&)>& change) { ChangeNativeConfigArray(Class, *Prop, Immediate, save, change); }

		// An empty element at the end, the array opened (OBSERVED)
		void ArrayAdd() override
		{
			Change(false, [](::Array<std::string>& list) { list.push_back({}); });
			if (IsExpanded())
				Rebuild();
			else
				Expand();
		}

		// The array cleared and closed (OBSERVED)
		void ArrayEmpty() override
		{
			Change(false, [](::Array<std::string>& list) { list.clear(); });
			Collapse();
		}

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override
		{
			size_t count = Values().size();
			for (size_t i = 0; i < count; i++)
				rows.push_back(std::make_unique<NativeElementRow>(this, i));
		}

	private:
		const NativeConfigClass* Class;
		const NativeConfigProperty* Prop;
		bool Immediate;
	};

	NativeElementRow::NativeElementRow(NativeArrayRow* array, size_t index) : Array(array), Index(index)
	{
		Caption = "[" + std::to_string(index) + "]";
	}

	std::string NativeElementRow::GetValue()
	{
		::Array<std::string> values = Array->Values();
		return Index < values.size() ? values[Index] : std::string();
	}

	bool NativeElementRow::SetValue(const std::string& text)
	{
		size_t index = Index;
		Array->Change(true, [&](::Array<std::string>& list) {
			if (index < list.size())
				list[index] = text;
		});
		return true;
	}

	// An empty element before this one (OBSERVED)
	void NativeElementRow::ElementInsert()
	{
		size_t index = Index;
		Array->Change(false, [=](::Array<std::string>& list) {
			if (index <= list.size())
				list.insert(list.begin() + index, std::string());
		});
		Array->Rebuild();
	}

	void NativeElementRow::ElementDelete()
	{
		size_t index = Index;
		Array->Change(false, [=](::Array<std::string>& list) {
			if (index < list.size())
				list.erase(list.begin() + index);
		});
		Array->Rebuild();
	}

	// UInput's Aliases[40]: a static array, its elements the struct Alias,
	// its members in the struct's order, Command and Alias
	class AliasMemberRow : public PropertyRow
	{
	public:
		AliasMemberRow(int index, bool command) : Index(index), Command(command)
		{
			Caption = command ? "Command" : "Alias";
		}

		std::string GetValue() override
		{
			NativeAlias alias = ReadNativeAlias(Index);
			return Command ? alias.Command : alias.Alias;
		}

		PropertyEditor GetEditor() override { return PropertyEditor::Text; }

		bool SetValue(const std::string& text) override
		{
			NativeAlias alias = ReadNativeAlias(Index);
			if (Command)
				alias.Command = text;
			else
				alias.Alias = text;
			SetNativeAlias(Index, alias);
			return true;
		}

	private:
		int Index;
		bool Command;
	};

	class AliasRow : public PropertyRow
	{
	public:
		AliasRow(int index) : Index(index)
		{
			Caption = "[" + std::to_string(index) + "]";
		}

		bool HasBox() override { return true; }
		std::string GetValue() override { return NativeAliasText(ReadNativeAlias(Index)); }

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override
		{
			rows.push_back(std::make_unique<AliasMemberRow>(Index, true));
			rows.push_back(std::make_unique<AliasMemberRow>(Index, false));
		}

	private:
		int Index;
	};

	class AliasArrayRow : public PropertyRow
	{
	public:
		AliasArrayRow() { Caption = "Aliases"; }
		bool HasBox() override { return true; }
		bool OpensByDoubleClick() override { return true; }
		std::string GetValue() override { return "..."; }

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override
		{
			for (int i = 0; i < 40; i++)
				rows.push_back(std::make_unique<AliasRow>(i));
		}
	};

	// A key's binding: the live input's at once, as SET Input sets it
	class RawKeyRow : public PropertyRow
	{
	public:
		RawKeyRow(const NativeConfigClass* cls, const NativeConfigProperty* prop, const std::string& key) : Class(cls), Prop(prop)
		{
			Caption = key;
		}

		std::string GetValue() override { return NativeConfigValue(Class, *Prop, Caption); }
		PropertyEditor GetEditor() override { return PropertyEditor::Text; }

		bool SetValue(const std::string& text) override
		{
			SetNativeConfigValue(Class, *Prop, Caption, text);
			return true;
		}

	private:
		const NativeConfigClass* Class;
		const NativeConfigProperty* Prop;
	};

	// A C++ class's page: its config properties in the line's category, or
	// all of them for a line with none. Its line's Immediate matters to
	// Engine.GameEngine's arrays alone: the classes DXEngine keeps an object
	// for (the client, the audio and render devices) are Immediate, and
	// their objects their defaults, as GET answers them; the others'
	// (Drivers' Engine.Engine, TCP/IP Network Play's IpDrv.TcpNetDriver)
	// defaults their sections alone, a net driver reading its section as it
	// starts
	class NativeClassRow : public PropertyRow
	{
	public:
		NativeClassRow(const std::string& caption, const NativeConfigClass* cls, const std::string& category, bool immediate) : Class(cls), Category(category), Immediate(immediate)
		{
			Caption = caption;
		}

		bool HasBox() override { return true; }
		bool HasValue() override { return false; }
		bool IsObjectRow() override { return true; }
		bool HasResetToDefaults() override { return true; }

		// As the original's ResetConfig of the class (Core.dll 0x10151ac0;
		// OBSERVED): the class's whole section copied from Default.ini or
		// DefUser.ini as it spells it, every category's, then the config read
		// again, the engine's object of the class taking each value; and the
		// file written at once, as the original's window has it written
		// (OBSERVED: Original.ini held Default.ini's section 2 s after the
		// click, before any other edit), ResetConfig itself flushing nothing
		void ResetToDefaults() override
		{
			ResetNativeConfig(Class);
			engine->FlushConfig();
			Rebuild();
		}

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override
		{
			for (const NativeConfigProperty* prop : AllNativeConfigProperties(Class))
			{
				if (!Category.empty() && !Same(Category, prop->Category))
					continue;
				switch (prop->PropType)
				{
				case NativeType::RawKeys:
					for (const std::string& key : NativeRawKeyNames())
						rows.push_back(std::make_unique<RawKeyRow>(Class, prop, key));
					break;
				case NativeType::Aliases:
					rows.push_back(std::make_unique<AliasArrayRow>());
					break;
				case NativeType::StringArray:
				case NativeType::NameArray:
					rows.push_back(std::make_unique<NativeArrayRow>(Class, prop, Immediate));
					break;
				default:
					rows.push_back(std::make_unique<NativeValueRow>(Class, prop));
					break;
				}
			}
			SortRows(rows);
		}

	private:
		const NativeConfigClass* Class;
		std::string Category;
		bool Immediate;
	};
}

/////////////////////////////////////////////////////////////////////////////
// A script class's rows and an object's

namespace
{
	// Whose values a script row shows and edits: a config page's class --
	// its defaults, an edit as GlobalSetProperty makes it --, or EDITACTOR's
	// object, held for the window as the original's window holds its
	// objects for the collector
	struct ScriptTarget
	{
		UClass* Class = nullptr;
		bool Immediate = false;
		std::shared_ptr<GCRoot<UObject>> Object;

		PropertyDataBlock* Block() const
		{
			if (Object)
			{
				UObject* obj = Object->get();
				return obj ? &obj->PropertyData : nullptr;
			}
			return Class ? &Class->PropertyData : nullptr;
		}

		// A change made where the target holds its values: in each object of
		// the class and the class's defaults, the config saved for an edit,
		// not for an array button's (OBSERVED), for a page; in the object,
		// told of it, for EDITACTOR's
		void Change(const std::function<void(PropertyDataBlock&)>& change, bool save = true) const
		{
			if (Object)
			{
				if (UObject* obj = Object->get())
				{
					change(obj->PropertyData);
					obj->PostEditChange();
				}
			}
			else if (Class)
			{
				engine->GlobalChangeProperty(Class, Immediate, change, save);
			}
		}
	};

	// One step down to a value: a field (the block's property, a struct's
	// member) at an element, or a dynamic array's element
	struct ValueStep
	{
		UProperty* Prop = nullptr;
		int Index = 0;
		bool Dynamic = false;
	};

	void* Locate(PropertyDataBlock& block, const std::vector<ValueStep>& steps)
	{
		if (!block.Data)
			return nullptr;
		uint8_t* data = static_cast<uint8_t*>(block.Data);
		bool top = true;
		for (const ValueStep& step : steps)
		{
			if (step.Dynamic)
			{
				ScriptArray* arr = reinterpret_cast<ScriptArray*>(data);
				if (step.Index < 0 || (size_t)step.Index >= arr->GetSize())
					return nullptr;
				data = static_cast<uint8_t*>(arr->GetItem(step.Index));
			}
			else
			{
				size_t offset = step.Prop->DataOffset.DataOffset + (size_t)step.Index * step.Prop->ElementPitch();
				if (top && offset + step.Prop->ElementSize() > block.Size)
					return nullptr;
				data += offset;
			}
			top = false;
		}
		return data;
	}

	bool IsColor(UProperty* prop)
	{
		auto s = UObject::TryCast<UStructProperty>(prop);
		return s && s->Struct && s->Struct->Name == NameString("Color");
	}

	std::unique_ptr<PropertyRow> MakeFieldRow(const ScriptTarget& target, const std::vector<ValueStep>& prefix, UProperty* prop);

	// A value: a scalar, a struct, a dynamic array or its element
	class ScriptValueRow : public PropertyRow
	{
	public:
		ScriptValueRow(const ScriptTarget& target, std::vector<ValueStep> steps, UProperty* leaf, const std::string& caption) : Target(target), Steps(std::move(steps)), Leaf(leaf)
		{
			Caption = caption;
		}

		bool HasBox() override
		{
			return UObject::IsType<UArrayProperty>(Leaf) || (UObject::IsType<UStructProperty>(Leaf) && !IsColor(Leaf));
		}

		bool OpensByDoubleClick() override { return UObject::IsType<UArrayProperty>(Leaf); }

		void* Data()
		{
			PropertyDataBlock* block = Target.Block();
			return block ? Locate(*block, Steps) : nullptr;
		}

		std::string GetValue() override
		{
			void* data = Data();
			if (!data)
				return {};
			if (auto b = UObject::TryCast<UBoolProperty>(Leaf))
				return b->GetBool(data) ? "True" : "False";
			if (auto c = UObject::TryCast<UClassProperty>(Leaf))
			{
				UObject* value = *static_cast<UObject**>(data);
				return value ? ClassCaption(value->GetPathName(), MetaClassName(c).c_str()) : std::string();
			}
			if (UObject::IsType<UArrayProperty>(Leaf))
				return "...";
			if (UObject::IsType<UStrProperty>(Leaf))
				return ScriptToUTF8(*static_cast<ScriptString*>(data));
			return Leaf->PrintValue(data);
		}

		bool IsObjectReference() override
		{
			return UObject::IsType<UObjectProperty>(Leaf) && !UObject::IsType<UClassProperty>(Leaf) && GetEditor() == PropertyEditor::Text;
		}

		PropertyEditor GetEditor() override
		{
			// An editconst property's value shown, not edited, as UnrealEd's
			// window keeps one (Object's Name and Class)
			if (!Steps.empty() && AnyFlags(Steps.front().Prop->PropFlags, PropertyFlags::EditConst))
				return PropertyEditor::None;
			if (IsColor(Leaf))
				return PropertyEditor::Color;
			if (UObject::IsType<UStructProperty>(Leaf) || UObject::IsType<UArrayProperty>(Leaf))
				return PropertyEditor::None;
			if (UObject::IsType<UBoolProperty>(Leaf) || UObject::IsType<UClassProperty>(Leaf))
				return PropertyEditor::Combo;
			if (auto b = UObject::TryCast<UByteProperty>(Leaf))
				return b->EnumType ? PropertyEditor::Combo : PropertyEditor::Slider;
			return PropertyEditor::Text;
		}

		std::vector<std::string> GetChoices() override
		{
			std::vector<std::string> choices;
			if (UObject::IsType<UBoolProperty>(Leaf))
				choices = { "False", "True" };
			else if (auto b = UObject::TryCast<UByteProperty>(Leaf); b && b->EnumType)
			{
				for (const NameString& name : b->EnumType->ElementNames)
					choices.push_back(name.ToString());
			}
			else if (auto c = UObject::TryCast<UClassProperty>(Leaf))
			{
				for (const ClassChoice& choice : ClassChoices(MetaClassName(c).c_str()))
					choices.push_back(choice.Caption);
			}
			return choices;
		}

		int GetChoice() override
		{
			void* data = Data();
			if (!data)
				return -1;
			if (auto c = UObject::TryCast<UClassProperty>(Leaf))
			{
				UObject* value = *static_cast<UObject**>(data);
				return value ? ClassChoiceIndex(ClassChoices(MetaClassName(c).c_str()), value->GetPathName()) : -1;
			}
			std::vector<std::string> choices = GetChoices();
			auto it = std::find(choices.begin(), choices.end(), GetValue());
			return it == choices.end() ? -1 : (int)(it - choices.begin());
		}

		bool GetColor(uint32_t& argb) override
		{
			void* data = Data();
			auto s = UObject::TryCast<UStructProperty>(Leaf);
			if (!data || !s || !IsColor(Leaf))
				return false;
			uint32_t r = 0, g = 0, b = 0, a = 0;
			for (UProperty* member : s->Struct->Properties)
			{
				uint8_t value = *(static_cast<uint8_t*>(data) + member->DataOffset.DataOffset);
				if (member->Name == NameString("R")) r = value;
				else if (member->Name == NameString("G")) g = value;
				else if (member->Name == NameString("B")) b = value;
				else if (member->Name == NameString("A")) a = value;
			}
			argb = (a << 24) | (r << 16) | (g << 8) | b;
			return true;
		}

		// The text into the value as the property's import takes it, where
		// the target holds it (a page's: GlobalSetProperty's way)
		bool SetValue(const std::string& input) override
		{
			std::string text = input;
			if (auto c = UObject::TryCast<UClassProperty>(Leaf))
				text = ClassFromCaption(ClassChoices(MetaClassName(c).c_str()), input);
			try
			{
				std::vector<ValueStep> steps = Steps;
				UProperty* leaf = Leaf;
				Target.Change([&](PropertyDataBlock& block) {
					if (void* data = Locate(block, steps))
						leaf->SetValueFromString(data, text);
				});
			}
			catch (const std::exception& e)
			{
				LogMessage("Could not set property '" + Leaf->Name.ToString() + "': " + e.what());
			}
			return true;
		}

		bool IsDynamicArray() override { return UObject::IsType<UArrayProperty>(Leaf); }
		bool IsDynamicElement() override { return !Steps.empty() && Steps.back().Dynamic; }

		// An empty element at the end, the array opened; Empty clears and
		// closes it; Insert puts an empty element before one, Delete removes
		// it -- the defaults' in memory, nothing saved (OBSERVED)
		void ArrayAdd() override
		{
			ChangeArray(Steps, [](ScriptArray& arr) { arr.Insert(arr.GetSize(), 1); });
			if (IsExpanded())
				Rebuild();
			else
				Expand();
		}

		void ArrayEmpty() override
		{
			ChangeArray(Steps, [](ScriptArray& arr) { arr.Clear(); });
			Collapse();
		}

		void ElementInsert() override
		{
			std::vector<ValueStep> arraySteps(Steps.begin(), Steps.end() - 1);
			size_t index = (size_t)Steps.back().Index;
			ChangeArray(arraySteps, [=](ScriptArray& arr) { arr.Insert(std::min(index, arr.GetSize()), 1); });
			if (GetParent())
				GetParent()->Rebuild();
		}

		void ElementDelete() override
		{
			std::vector<ValueStep> arraySteps(Steps.begin(), Steps.end() - 1);
			size_t index = (size_t)Steps.back().Index;
			ChangeArray(arraySteps, [=](ScriptArray& arr) { if (index < arr.GetSize()) arr.Remove(index, 1); });
			if (GetParent())
				GetParent()->Rebuild();
		}

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override
		{
			if (auto a = UObject::TryCast<UArrayProperty>(Leaf))
			{
				void* data = Data();
				size_t count = data ? static_cast<ScriptArray*>(data)->GetSize() : 0;
				for (size_t i = 0; i < count; i++)
				{
					std::vector<ValueStep> steps = Steps;
					steps.push_back({ Leaf, (int)i, true });
					rows.push_back(std::make_unique<ScriptValueRow>(Target, steps, a->Inner, "[" + std::to_string(i) + "]"));
				}
			}
			else if (auto s = UObject::TryCast<UStructProperty>(Leaf))
			{
				// A struct's members in its order (X, Y, Z; Pitch, Yaw, Roll)
				if (s->Struct)
				{
					for (UProperty* member : s->Struct->Properties)
						rows.push_back(MakeFieldRow(Target, Steps, member));
				}
			}
		}

	private:
		static std::string MetaClassName(UClassProperty* prop)
		{
			UClass* meta = prop->MetaClass;
			return meta && meta->package ? meta->package->GetPackageName().ToString() + "." + meta->Name.ToString() : std::string();
		}

		void ChangeArray(const std::vector<ValueStep>& arraySteps, const std::function<void(ScriptArray&)>& change)
		{
			try
			{
				Target.Change([&](PropertyDataBlock& block) {
					if (void* data = Locate(block, arraySteps))
						change(*static_cast<ScriptArray*>(data));
				}, false);
			}
			catch (const std::exception& e)
			{
				LogMessage("Could not change array '" + Caption + "': " + e.what());
			}
		}

		ScriptTarget Target;
		std::vector<ValueStep> Steps;
		UProperty* Leaf;
	};

	// A static array: "..." and a box, its elements [0] to [n-1] (IPPolicies)
	class ScriptStaticArrayRow : public PropertyRow
	{
	public:
		ScriptStaticArrayRow(const ScriptTarget& target, std::vector<ValueStep> prefix, UProperty* prop) : Target(target), Prefix(std::move(prefix)), Prop(prop)
		{
			Caption = prop->Name.ToString();
		}

		bool HasBox() override { return true; }
		bool OpensByDoubleClick() override { return true; }
		std::string GetValue() override { return "..."; }

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override
		{
			for (int i = 0; i < Prop->ArrayDimension; i++)
			{
				std::vector<ValueStep> steps = Prefix;
				steps.push_back({ Prop, i, false });
				rows.push_back(std::make_unique<ScriptValueRow>(Target, steps, Prop, "[" + std::to_string(i) + "]"));
			}
		}

	private:
		ScriptTarget Target;
		std::vector<ValueStep> Prefix;
		UProperty* Prop;
	};

	std::unique_ptr<PropertyRow> MakeFieldRow(const ScriptTarget& target, const std::vector<ValueStep>& prefix, UProperty* prop)
	{
		if (prop->ArrayDimension > 1)
			return std::make_unique<ScriptStaticArrayRow>(target, prefix, prop);
		std::vector<ValueStep> steps = prefix;
		steps.push_back({ prop, 0, false });
		return std::make_unique<ScriptValueRow>(target, steps, prop, prop->Name.ToString());
	}

	// A class's properties a page or an object shows: each name once, the
	// class's own of a name its super declares too, as the original's field
	// walk finds a name
	std::vector<UProperty*> ClassProperties(UClass* cls, const std::function<bool(UProperty*)>& shown)
	{
		std::vector<UProperty*> props;
		for (UProperty* prop : cls->Properties)
		{
			if (cls->FindProperty(prop->Name) == prop && shown(prop))
				props.push_back(prop);
		}
		return props;
	}

	// A script class's page: its config properties in the line's category,
	// or all of them for a line with none (Game Settings' GameInfo)
	class ScriptClassRow : public PropertyRow
	{
	public:
		ScriptClassRow(const std::string& caption, UClass* cls, const std::string& category, bool immediate) : Category(category)
		{
			Caption = caption;
			Target.Class = cls;
			Target.Immediate = immediate;
		}

		bool HasBox() override { return true; }
		bool HasValue() override { return false; }
		bool IsObjectRow() override { return true; }
		bool HasResetToDefaults() override { return true; }

		// The original's ResetConfig of the class (Core.dll 0x10151ac0), as
		// the script's ResetConfig has it, the file written at once as for a
		// C++ class's page (OBSERVED there)
		void ResetToDefaults() override
		{
			NObject::ResetConfig(Target.Class);
			engine->FlushConfig();
			Rebuild();
		}

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override
		{
			NameString category(Category);
			for (UProperty* prop : ClassProperties(Target.Class, [&](UProperty* p) {
				return AnyFlags(p->PropFlags, PropertyFlags::Config | PropertyFlags::GlobalConfig) && (Category.empty() || p->Category == category);
			}))
				rows.push_back(MakeFieldRow(Target, {}, prop));
			SortRows(rows);
		}

	private:
		ScriptTarget Target;
		std::string Category;
	};

	// EDITACTOR's category: the object's editable properties of it, and
	// under None every property not editable, as the original's window has
	// them (OBSERVED: JCDentonMale's None holds 521 rows, aBaseX to
	// A property's category in EDITACTOR's window: its own if editable, None
	// for any other
	NameString ObjectCategoryOf(UProperty* prop)
	{
		return AnyFlags(prop->PropFlags, PropertyFlags::Edit) ? prop->Category : NameString("None");
	}

	// ZoomLevel, the localized messages and the references among them)
	class ObjectCategoryRow : public PropertyRow
	{
	public:
		ObjectCategoryRow(const ScriptTarget& target, const NameString& category) : Target(target), Category(category)
		{
			Caption = category.ToString();
		}

		bool HasBox() override { return true; }
		bool HasValue() override { return false; }

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override
		{
			UObject* obj = Target.Object ? Target.Object->get() : nullptr;
			if (!obj)
				return;
			for (UProperty* prop : ClassProperties(obj->Class, [&](UProperty* p) { return ObjectCategoryOf(p) == Category; }))
				rows.push_back(MakeFieldRow(Target, {}, prop));
			SortRows(rows);
		}

	private:
		ScriptTarget Target;
		NameString Category;
	};

	class FailedLoadRow : public PropertyRow
	{
	public:
		FailedLoadRow(const std::string& caption) { Caption = caption; }
		bool HasValue() override { return false; }
	};

	void MakePreferencesRows(const std::string& parent, std::vector<std::unique_ptr<PropertyRow>>& rows);

	// A Preferences line with no class: its box, the lines under its caption
	class PreferencesCategoryRow : public PropertyRow
	{
	public:
		PreferencesCategoryRow(const std::string& caption) { Caption = caption; }
		bool HasBox() override { return true; }
		bool HasValue() override { return false; }

	protected:
		void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) override { MakePreferencesRows(Caption, rows); }
	};

	// A line's class: a C++ class DXEngine stands in for, a script class
	// loaded as the original's StaticLoadClass loads it, or one that does
	// not load, shown as "<class> (failed load)" (Window.int's
	// FailedConfigLoad; OBSERVED: GlideDrv.GlideRenderDevice and
	// MeTaLDrv.MeTaLRenderDevice, whose DLLs do not load)
	std::unique_ptr<PropertyRow> MakeClassRow(const IntPreferences& pref)
	{
		if (const NativeConfigClass* native = FindNativeConfigClass(pref.Class))
			return std::make_unique<NativeClassRow>(pref.Caption, native, pref.Category, pref.Immediate);
		UClass* cls = nullptr;
		try
		{
			cls = engine->packages->FindClass(pref.Class);
		}
		catch (...)
		{
			cls = nullptr;
		}
		if (cls)
			return std::make_unique<ScriptClassRow>(pref.Caption, cls, pref.Category, pref.Immediate);
		std::string text = Localized("Window", "Errors", "FailedConfigLoad", "%s (failed load)");
		size_t at = text.find("%s");
		if (at != std::string::npos)
			text.replace(at, 2, pref.Class);
		return std::make_unique<FailedLoadRow>(text);
	}

	// The lines under a caption: each caption once, its first line's, in
	// the order of their captions (OBSERVED: the top items Advanced to
	// Rendering; Rendering's devices by their captions, 3dfx Glide support's
	// first), a line's class loaded as its parent opens
	void MakePreferencesRows(const std::string& parent, std::vector<std::unique_ptr<PropertyRow>>& rows)
	{
		std::vector<const IntPreferences*> lines;
		for (const IntPreferences& pref : engine->packages->GetIntPreferences())
		{
			if (!Same(pref.Parent, parent))
				continue;
			bool seen = false;
			for (const IntPreferences* line : lines)
				seen = seen || Same(line->Caption, pref.Caption);
			if (!seen)
				lines.push_back(&pref);
		}
		std::stable_sort(lines.begin(), lines.end(), [](const IntPreferences* a, const IntPreferences* b) {
			return NameString::Folded(a->Caption) < NameString::Folded(b->Caption);
		});
		for (const IntPreferences* pref : lines)
		{
			if (pref->Class.empty())
				rows.push_back(std::make_unique<PreferencesCategoryRow>(pref->Caption));
			else
				rows.push_back(MakeClassRow(*pref));
		}
	}
}

std::vector<std::unique_ptr<PropertyRow>> MakeAdvancedOptionsRows()
{
	std::vector<std::unique_ptr<PropertyRow>> rows;
	MakePreferencesRows("Advanced Options", rows);
	return rows;
}

static void DumpRow(PropertyRow* row)
{
	static const char* const editors[] = { "none", "edit", "combo", "slider", "color" };
	std::string line = "PROPDUMP: " + std::string(row->GetDepth() * 2, ' ') + row->GetCaption();
	if (row->HasValue())
		line += " = '" + row->GetValue() + "' " + editors[(int)row->GetEditor()];
	if (row->GetEditor() == PropertyEditor::Combo)
	{
		line += " [";
		for (const std::string& choice : row->GetChoices())
			line += choice + "|";
		line += "] " + std::to_string(row->GetChoice());
	}
	if (row->HasResetToDefaults())
		line += " +reset";
	if (row->IsDynamicArray())
		line += " +empty+add";
	if (row->IsDynamicElement())
		line += " +delete+insert";
	LogMessage(line);
	if (row->HasBox() && !row->IsExpanded())
	{
		row->Expand();
		for (const auto& child : row->GetChildren())
			DumpRow(child.get());
		row->Collapse();
	}
}

void DumpPropertyRows(const std::vector<std::unique_ptr<PropertyRow>>& rows)
{
	for (const auto& row : rows)
		DumpRow(row.get());
}

std::vector<std::unique_ptr<PropertyRow>> MakeObjectRows(UObject* obj)
{
	std::vector<std::unique_ptr<PropertyRow>> rows;
	if (!obj)
		return rows;
	ScriptTarget target;
	target.Object = std::make_shared<GCRoot<UObject>>(obj);
	std::vector<NameString> categories;
	for (UProperty* prop : ClassProperties(obj->Class, [](UProperty*) { return true; }))
	{
		NameString category = ObjectCategoryOf(prop);
		if (std::find(categories.begin(), categories.end(), category) == categories.end())
			categories.push_back(category);
	}
	for (const NameString& category : categories)
		rows.push_back(std::make_unique<ObjectCategoryRow>(target, category));
	SortRows(rows);
	return rows;
}
