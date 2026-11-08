
#include "Precomp.h"
#include "Package.h"
#include "PackageWriter.h"
#include "PackageStream.h"
#include "PackageManager.h"
#include "Utils/File.h"
#include "Utils/StrTools.h"
#include "Packages/Core/UObject.h"
#include "Packages/Core/UClass.h"

Package::Package(PackageManager* packageManager, const NameString& name, const std::string& filepath) : Packages(packageManager), Name(name), FilePath(filepath)
{
	// Before the tables, whose names a save file makes whatever their flags
	FileName = fs::path(FilePath).filename().string();
	FileExtension = fs::path(FilePath).extension().string();

	if (!filepath.empty())
		ReadTables();
	else
		Version = 68; // a package born empty carries the version the writer emits, or its own save cannot be read back

	// Register native classes not listed in the package
	for (const auto& it : packageManager->NativeClasses)
	{
		const NativeClass& nativeClass = it.second;
		if (nativeClass.Package == name)
		{
			int objref = FindObjectReference("Class", nativeClass.Name, {});
			if (objref == 0)
			{
				if (NameHash.find(nativeClass.Name) == NameHash.end())
				{
					NameTableEntry nameentry;
					nameentry.Flags = 0;
					nameentry.Name = nativeClass.Name;
					NameTable.push_back(nameentry);
					NameHash[nativeClass.Name] = (int)NameTable.size() - 1;
				}

				ExportTableEntry entry;
				entry.ObjClass = 0;
				entry.ObjBase = nativeClass.Base.IsNone() ? 0 : FindObjectReference("Class", nativeClass.Base, {});
				entry.ObjOuter = 0;
				entry.ObjName = NameHash[nativeClass.Name];
				entry.ObjFlags = ObjectFlags::Native;
				entry.ObjSize = 0;
				entry.ObjOffset = 0;
				ExportTable.push_back(entry);
			}
		}
	}

	ExportObjects.resize(ExportTable.size());
}

Package::~Package()
{
}

void Package::LoadAll()
{
	for (size_t i = 0; i < ExportTable.size(); i++)
	{
		GetUObject((int)i + 1);
	}
}

void Package::Save(UObject* object, const std::string& filename)
{
	if (object && object->package != this)
		Exception::Throw("Object does not belong to this package");

	PackageWriter writer(this);
	writer.Save(object, filename);
}

UObject* Package::NewObject(const NameString& objname, UClass* objclass, ObjectFlags flags, bool initProperties)
{
	if (!objclass)
		Exception::Throw("Package.NewObject called with a null objclass!");

	UObject* obj = Packages->CreateNativeObject(objname, objclass, flags);
	if (!obj)
		Exception::Throw("Could not find the native class for " + objname.ToString());
	obj->package = this;
	ObjectNames[objname.GetCompareIndex()]++;
	obj->NameCounted = true;
	if (initProperties)
	{
		obj->PropertyData.Init(objclass);
		obj->SetObject("Class", obj->Class);
		obj->SetName("Name", obj->Name);
		obj->SetInt("ObjectFlags", (int)obj->Flags);
		// In the package until its maker gives it an outer of its own: an
		// actor its level's, new's the one given (Transient for None)
		obj->SetObject("Outer", GetPackageObject());
		obj->InitNativeDefaults();
	}
	return obj;
}

UObject* Package::GetPackageObject()
{
	if (!PackageObject)
	{
		UClass* cls = Packages->GetPackage("Core")->GetClass("Package");
		cls->LoadNow();
		UObject* obj = Packages->CreateNativeObject(Name, cls, ObjectFlags::Public);
		obj->package = this;
		obj->PropertyData.Init(cls);
		obj->SetObject("Class", cls);
		obj->SetName("Name", Name);
		obj->SetInt("ObjectFlags", (int)obj->Flags);
		PackageObject = obj;
	}
	return PackageObject;
}

void Package::Mark(GCMarker& marker)
{
	marker.SetField("PackageObject");
	marker.MarkConst(PackageObject);
}

NameString Package::MakeUniqueObjectName(UClass* cls)
{
	std::string base = cls->Name.ToString();
	while (!base.empty() && base.back() >= '0' && base.back() <= '9')
		base.pop_back();

	if (ExportNamesBuiltFrom != ExportTable.size())
	{
		ExportNameIndexes.clear();
		for (const ExportTableEntry& entry : ExportTable)
			ExportNameIndexes.insert(entry.ObjName);
		ExportNamesBuiltFrom = ExportTable.size();
	}

	while (true)
	{
		NameString candidate = NameString::Collectable(base + std::to_string(cls->ClassUnique++));
		auto it = NameHash.find(candidate);
		bool exported = it != NameHash.end() && ExportNameIndexes.count(it->second);
		auto live = ObjectNames.find(candidate.GetCompareIndex());
		bool borne = live != ObjectNames.end() && live->second > 0;
		if (!exported && !borne)
			return candidate;
	}
}

void Package::ForgetObjectName(const NameString& name)
{
	auto it = ObjectNames.find(name.GetCompareIndex());
	if (it != ObjectNames.end() && --it->second <= 0)
		ObjectNames.erase(it);
}

void Package::AddRuntimeExport(UObject* obj)
{
	int nameIndex;
	auto it = NameHash.find(obj->Name);
	if (it != NameHash.end())
	{
		nameIndex = it->second;
	}
	else
	{
		nameIndex = (int)NameTable.size();
		NameTableEntry nameEntry;
		nameEntry.Name = obj->Name;
		nameEntry.Flags = 0;
		NameTable.push_back(nameEntry);
		NameHash[obj->Name] = nameIndex;
	}

	ExportTableEntry entry = {};
	entry.ObjClass = 0; // a class
	entry.ObjName = nameIndex;
	entry.ObjFlags = obj->Flags;
	ExportTable.push_back(entry);
	ExportObjects.push_back(obj);
	obj->package = this;
	obj->exportIndex = (uint32_t)(ExportTable.size() - 1);
}

void Package::LoadExportObject(int index)
{
	// An export whose class, outer or base is itself, or a ring of them,
	// recursed here without end (the slot is filled only after the load):
	// a depth past 256 is such a ring, refused with the package's name
	static int depth = 0;
	struct DepthGuard { int& d; DepthGuard(int& d) : d(d) { d++; } ~DepthGuard() { d--; } } guard(depth);
	if (depth > 256)
		Exception::Throw("Package " + Name.ToString() + ": the exports' classes, outers or bases never end");

	const ExportTableEntry* entry = &ExportTable[index];

	SetDelayLoadActive delayload(Packages);

	NameString objname = GetName(entry->ObjName);

	if (entry->ObjClass != 0)
	{
		UClass* objclass = UObject::Cast<UClass>(GetUObject(entry->ObjClass));
		if (!objclass)
		{
			Exception::Throw("Could not find the object class for " + objname.ToString());
		}

		UObject* outer = entry->ObjOuter != 0 ? GetUObject(entry->ObjOuter) : nullptr;
		ExportObjects[index] = NewObject(objname, objclass, ExportTable[index].ObjFlags, false);
		ExportObjects[index]->DelayLoad.reset(new ObjectDelayLoad(this, index, objname, objclass, outer));
		Packages->delayLoads.push_back(ExportObjects[index]);
	}
	else
	{
		UClass* objbase = UObject::Cast<UClass>(GetUObject(entry->ObjBase));
		if (!objbase && objname != "Object")
			objbase = UObject::Cast<UClass>(Packages->GetPackage("Core")->GetUObject("Class", "Object"));
		auto obj = GC::Alloc<UClass>(objname, objbase, ExportTable[index].ObjFlags);
		// A C++ class its file lacks has its C++ flags, as the original's
		if (index >= FileExportCount)
		{
			auto native = Packages->NativeClasses.find(objname);
			if (native != Packages->NativeClasses.end() && native->second.Package == Name)
				obj->ClsFlags = (ClassFlags)((uint32_t)obj->ClsFlags | native->second.ClsFlags);
		}
		ExportObjects[index] = obj;
		ExportObjects[index]->DelayLoad.reset(new ObjectDelayLoad(this, index, objname, objbase, nullptr));
		Packages->delayLoads.push_back(ExportObjects[index]);
		obj->Class = UObject::Cast<UClass>(Packages->GetPackage("Core")->GetUObject("Class", "Class"));
	}
}

UObject* Package::GetUObject(int objref)
{
	if (objref > 0) // Export table object
	{
		int index = objref - 1;

		if ((size_t)index >= ExportObjects.size())
			Exception::Throw("Invalid object reference");

		// A file's export outside the context is never made, a reference to
		// it None, as the original's linker makes none outside its context,
		// the game's client and server, a dedicated server's alone (Core's
		// ULinkerLoad::CreateExport; the context set at 0x101600d5;
		// LoadContext): the editor's alone -- a map's brushes, their models
		// and polys, its cameras and script text, a class's ScriptText --
		// made here before (bugs.md VM.25), protected packages' ScriptText,
		// which is for no context, and on a dedicated server those for the
		// client alone; a game with a client makes those (bugs.md ENG.23).
		// DXEngine's own saves wrote spawned actors without these flags, so a
		// save's exports are all made.
		if (!ExportObjects[index] && index < FileExportCount && !AnyFlags(ExportTable[index].ObjFlags, LoadContext()) && !IsSaveFile())
			return nullptr;

		// Nor is one of a class named Camera, a map's or a save's: the
		// original's CreateExport makes none whatever its flags (Core.dll
		// 0x1012e230), a reference to it None
		if (!ExportObjects[index] && ExportTable[index].ObjClass != 0)
		{
			static const NameString camera("Camera");
			int objclass = ExportTable[index].ObjClass;
			if (GetName(objclass < 0 ? GetImportEntry(objclass)->ObjName : GetExportEntry(objclass)->ObjName) == camera)
				return nullptr;
		}

		if (!ExportObjects[index])
			LoadExportObject(index);

		if (Packages->delayLoadActive == 0)
			Packages->DelayLoadNow();

		UObject* object = ExportObjects[index];
		object->package = this;
		object->exportIndex = index;
		return object;
	}
	else if (objref < 0) // Import table object
	{
		// The export the import's check found (VerifyImport); none for a
		// package, there being no UObject for those currently, and for an
		// object of a safe-replace class its package lacks
		int index = -objref - 1;
		GetImportEntry(objref);
		for (int pass = 0; pass < 2; pass++)
		{
			VerifyImport(index);
			NameString from = ImportSources[index].Package;
			int exportRef = ImportSources[index].ExportRef;
			if (exportRef == 0)
				return nullptr;
			Package* source = Packages->GetPackage(from);
			if ((size_t)exportRef <= source->ExportTable.size() && source->GetName(source->ExportTable[exportRef - 1].ObjName) == GetName(ImportTable[index].ObjName))
				return source->GetUObject(exportRef);
			// Its package opened again from another file since it was
			// checked (a download let go): every import checked again
			ImportSources.clear();
		}
		return nullptr;
	}
	else
	{
		return nullptr;
	}
}

ObjectFlags Package::LoadContext() const
{
	return Packages->launchInfo.dedicatedServer ? ObjectFlags::LoadForServer : ObjectFlags::LoadForClient | ObjectFlags::LoadForServer;
}

bool Package::IsSaveFile() const
{
	std::string ext = Packages->GetSaveExtension();
	if (!ext.empty() && ext.front() != '.')
		ext = "." + ext;
	return !ext.empty() && StrTools::equals_ignore_case(FileExtension, ext);
}

UObject* Package::GetUObject(const NameString& className, const NameString& objectName, const NameString& group, bool ignoreGroup)
{
	return GetUObject(FindObjectReference(className, objectName, group, ignoreGroup));
}

UObject* Package::LoadObjectAnyGroup(UClass* cls, const NameString& objectName)
{
	if (!cls)
		return nullptr;

	// The export's class must be the one asked for, by its name and its
	// package (Core's FindExportIndex); a Mesh asked for is looked for again
	// as a LodMesh, its package the same.
	auto exportClassIs = [&](const ExportTableEntry& entry, const NameString& className) {
		NameString name, package;
		if (entry.ObjClass == 0)
		{
			name = "Class";
			package = "Core";
		}
		else if (entry.ObjClass < 0)
		{
			ImportTableEntry* classImport = GetImportEntry(entry.ObjClass);
			name = GetName(classImport->ObjName);
			ImportTableEntry* outer = classImport->ObjOuter < 0 ? GetImportEntry(classImport->ObjOuter) : nullptr;
			while (outer && outer->ObjOuter < 0)
				outer = GetImportEntry(outer->ObjOuter);
			if (outer)
				package = GetName(outer->ObjName);
		}
		else
		{
			name = GetName(GetExportEntry(entry.ObjClass)->ObjName);
			package = Name;
		}
		return name == className && cls->package && package == cls->package->GetPackageName();
	};
	auto findExact = [&](const NameString& className) -> int {
		for (size_t index = 0; index < ExportTable.size(); index++)
		{
			const ExportTableEntry& entry = ExportTable[index];
			if (GetName(entry.ObjName) == objectName && exportClassIs(entry, className))
				return (int)index + 1;
		}
		return 0;
	};

	int objref = findExact(cls->Name);
	if (objref == 0 && cls->Name == "Mesh")
		objref = findExact("LodMesh");
	if (objref != 0)
		return GetUObject(objref);

	// Core's StaticFindObject on what is loaded, its outer the package.
	for (size_t index = 0; index < ExportTable.size() && index < ExportObjects.size(); index++)
	{
		UObject* obj = ExportObjects[index];
		if (!obj || ExportTable[index].ObjOuter != 0 || GetName(ExportTable[index].ObjName) != objectName)
			continue;
		for (UClass* c = obj->Class; c; c = static_cast<UClass*>(c->BaseStruct))
		{
			if (c == cls)
				return GetUObject((int)index + 1);
		}
	}
	return nullptr;
}

UClass* Package::GetClass(const NameString& className)
{
	return UObject::Cast<UClass>(GetUObject("Class", className));
}

// Core's error text of the key (its .int's [Errors]), each %s given in turn,
// the English text where no file gives it
static std::string CoreError(PackageManager* packages, const char* key, const char* fallback, std::initializer_list<std::string> args)
{
	std::string format = packages->Localize("Core", "Errors", key);
	if (format.empty())
		format = fallback;
	std::string text;
	auto arg = args.begin();
	for (size_t i = 0; i < format.size(); i++)
	{
		if (format[i] == '%' && i + 1 < format.size() && format[i + 1] == 's')
		{
			if (arg != args.end())
				text += *arg++;
			i++;
		}
		else
		{
			text += format[i];
		}
	}
	return text;
}

void Package::VerifyImports()
{
	for (size_t i = 0; i < ImportTable.size(); i++)
		VerifyImport((int)i);
}

// The original's VerifyImport (Core.dll 0x10160fd0), each import once. A
// package's file is opened, as GetPackageLinker opens it (LOAD_Throw):
// UnrealI's, not there, is UnrealShare's (0x101610cd), and Core's
// PackageNotFound refuses this package without it. Any other import's outer
// is checked first; then, in its top package, the export of its name, of
// exactly its class -- the class's name and its package, UnrealShare's
// standing for UnrealI's --, whose outer is the export its own outer found or
// none, a top-level one standing for any group, the table's last first (its
// hash chain); a Mesh asked for, then a LodMesh. One that is not public
// refuses this package (Core's FailedImportPrivate). With none found, one
// right in UnrealI is looked for in UnrealShare (0x10161669); one whose class
// is in memory and of CLASS_SafeReplace (a texture, palette, sound or song)
// is None without a word; any other refuses this package (Core's
// FailedImport), but in a save, where it is None too (bugs.md D.75): the
// camera of a conversation going on as its level was left, which ConSys
// makes in package DeusEx, is one, the original refusing the level's save.
// A C++ class its file lacks is found among the native classes added to its
// package (the constructor's), as the original finds the class in memory.
void Package::VerifyImport(int index, int depth)
{
	if (ImportSources.size() < ImportTable.size())
		ImportSources.resize(ImportTable.size());
	if (ImportSources[index].Verified)
		return;
	// An import whose outer is itself, or a ring of them, checked without end
	if (depth > 64)
		Exception::Throw("Package " + Name.ToString() + ": an import's outers never end");

	const ImportTableEntry entry = ImportTable[index];
	const NameString objectName = GetName(entry.ObjName);
	const NameString classPackage = GetName(entry.ClassPackage);
	NameString className = GetName(entry.ClassName);
	ImportSource source;
	source.Verified = true;

	// One with a name of None is not looked for, as the original's passes it
	// over: an object of it is None
	if (objectName.IsNone() || className.IsNone() || classPackage.IsNone())
	{
		ImportSources[index] = source;
		return;
	}

	static const NameString unrealI("UnrealI"), unrealShare("UnrealShare");
	auto there = [&](const NameString& package) { return Packages->IsPackageLoaded(package) || !Packages->FindPackageFile(package, {}).empty(); };
	if (entry.ObjOuter == 0)
	{
		NameString package = objectName;
		if (!there(package) && package == unrealI)
			package = unrealShare;
		if (!there(package))
			throw PackageImportError(CoreError(Packages, "PackageNotFound", "Can't find file for package '%s'", { package.ToString() }), package.ToString());
		Packages->GetPackage(package);
		source.Package = package;
		ImportSources[index] = source;
		return;
	}

	// Its outer an import, as the original's asserts; checked first
	if (entry.ObjOuter > 0)
		Exception::Throw("Package " + Name.ToString() + ": an import inside an export");
	GetImportEntry(entry.ObjOuter);
	int outer = -entry.ObjOuter - 1;
	VerifyImport(outer, depth + 1);
	const ImportSource outerSource = ImportSources[outer];
	// Where the outer has no package (a name of None), the original asserts:
	// the import is None
	if (outerSource.Package.IsNone())
	{
		ImportSources[index] = source;
		return;
	}

	// How deep the import is in its top package, which its full name begins
	// with
	int importDepth = 1;
	const ImportTableEntry* top = GetImportEntry(entry.ObjOuter);
	for (; top->ObjOuter != 0; top = GetImportEntry(top->ObjOuter))
		importDepth++;
	NameString topName = GetName(top->ObjName);

	// The export's outer the one its outer found; a package's, the top
	const int outerExport = ImportTable[outer].ObjOuter != 0 ? outerSource.ExportRef : 0;
	NameString packageName = outerSource.Package;
	auto search = [&](Package* from, int within) -> int
	{
		static const NameString mesh("Mesh"), lodMesh("LodMesh");
		int found = 0;
		for (int pass = 0; pass < 2; pass++)
		{
			int match = from->FindImportedExport(objectName, className, classPackage, within);
			if (match != 0)
			{
				if (match <= from->FileExportCount && !AnyFlags(from->ExportTable[match - 1].ObjFlags, ObjectFlags::Public))
					throw PackageImportError(CoreError(Packages, "FailedImportPrivate", "Can't import private object %s %s", { className.ToString(), ImportFullName(index, className, topName) }));
				found = match;
			}
			if (className != mesh)
				break;
			className = lodMesh;
		}
		return found;
	};
	int found = search(Packages->GetPackage(packageName), outerExport);

	if (found == 0 && importDepth == 1 && topName == unrealI)
	{
		if (!there(unrealShare))
			throw PackageImportError(CoreError(Packages, "PackageNotFound", "Can't find file for package '%s'", { unrealShare.ToString() }), unrealShare.ToString());
		packageName = topName = unrealShare;
		found = search(Packages->GetPackage(packageName), 0);
	}

	if (found == 0)
	{
		// The import's class as the original finds it in memory: a native
		// class always, as a C++ one is there, any other once made
		bool safeReplace = false;
		auto native = Packages->NativeClasses.find(className);
		bool isNative = native != Packages->NativeClasses.end() && native->second.Package == classPackage;
		if (isNative || Packages->IsPackageLoaded(classPackage))
		{
			Package* classes = Packages->GetPackage(classPackage);
			int classRef = classes->FindObjectReference("Class", className, {});
			if (classRef != 0 && (isNative || classes->GetLoadedExport(classRef - 1)))
			{
				// Its flag a class's own or one it extends', as a subclass
				// takes its base's (a FireTexture a Texture's)
				for (UStruct* s = UObject::TryCast<UClass>(classes->GetUObject(classRef)); s && !safeReplace; s = s->BaseStruct)
				{
					if (UClass* cls = UObject::TryCast<UClass>(s))
					{
						cls->LoadNow();
						safeReplace = (cls->ClsFlags & ClassFlags::SafeReplace) != 0;
					}
				}
			}
		}
		if (!safeReplace && !IsSaveFile())
			throw PackageImportError(CoreError(Packages, "FailedImport", "Can't find %s in file '%s'", { className.ToString(), ImportFullName(index, className, topName) }));
	}

	source.Package = packageName;
	source.ExportRef = found;
	ImportSources[index] = source;
}

int Package::FindImportedExport(const NameString& objectName, const NameString& className, const NameString& classPackage, int outerExport)
{
	static const NameString unrealI("UnrealI"), unrealShare("UnrealShare");
	const Array<int>* named = ExportsNamed(objectName);
	if (!named)
		return 0;
	for (size_t i = named->size(); i-- > 0;)
	{
		const ExportTableEntry& entry = ExportTable[(*named)[i]];
		if (ExportClassName(entry) != className)
			continue;
		NameString package = ExportClassPackage(entry);
		if (package != classPackage && !(classPackage == unrealI && package == unrealShare))
			continue;
		if (entry.ObjOuter != 0 && entry.ObjOuter != outerExport)
			continue;
		return (*named)[i] + 1;
	}
	return 0;
}

NameString Package::ExportClassName(const ExportTableEntry& entry)
{
	static const NameString classClass("Class");
	if (entry.ObjClass == 0)
		return classClass;
	if (entry.ObjClass < 0)
		return GetName(GetImportEntry(entry.ObjClass)->ObjName);
	return GetName(GetExportEntry(entry.ObjClass)->ObjName);
}

// A class of this package's own is this package's; an imported class's, the
// import its outer names (none where it has none, where the original reads
// past its table)
NameString Package::ExportClassPackage(const ExportTableEntry& entry)
{
	static const NameString core("Core");
	if (entry.ObjClass == 0)
		return core;
	if (entry.ObjClass > 0)
		return Name;
	const ImportTableEntry* classImport = GetImportEntry(entry.ObjClass);
	if (classImport->ObjOuter >= 0)
		return {};
	return GetName(GetImportEntry(classImport->ObjOuter)->ObjName);
}

std::string Package::ImportFullName(int index, const NameString& className, const NameString& topPackage)
{
	std::string path;
	int ref = -index - 1;
	for (int outers = 0; ref < 0 && outers <= 64; outers++)
	{
		const ImportTableEntry* entry = GetImportEntry(ref);
		// The top the one the import was looked for in (UnrealShare for UnrealI)
		std::string name = entry->ObjOuter == 0 ? topPackage.ToString() : GetName(entry->ObjName).ToString();
		path = path.empty() ? name : name + "." + path;
		ref = entry->ObjOuter;
	}
	return className.ToString() + " " + path;
}

const Array<int>* Package::ExportsNamed(const NameString& name)
{
	if (ExportsByNameBuiltFrom != ExportTable.size())
	{
		ExportsByName.clear();
		for (size_t i = 0; i < ExportTable.size(); i++)
			ExportsByName[GetName(ExportTable[i].ObjName).GetCompareIndex()].push_back((int)i);
		ExportsByNameBuiltFrom = ExportTable.size();
	}
	auto it = ExportsByName.find(name.GetCompareIndex());
	return it != ExportsByName.end() ? &it->second : nullptr;
}

int Package::FindObjectReference(const NameString& className, const NameString& objectName, const NameString& group, bool ignoreGroup)
{
	bool isClass = className == "Class";

	size_t count = ExportTable.size();
	for (size_t index = 0; index < count; index++)
	{
		ExportTableEntry& entry = ExportTable[index];
		if (GetName(entry.ObjName) != objectName)
			continue;

		if (!ignoreGroup)
		{
			// Find the path for the outer object
			std::string entryGroup;
			int outerRef = entry.ObjOuter;
			while (outerRef != 0)
			{
				if (!entryGroup.empty())
					entryGroup += '.';
				if (outerRef > 0)
				{
					auto outerEntry = GetExportEntry(outerRef);
					entryGroup += GetName(outerEntry->ObjName).ToString();
					outerRef = outerEntry->ObjOuter;
				}
				else// if (outerRef < 0)
				{
					auto outerEntry = GetImportEntry(outerRef);
					entryGroup += GetName(outerEntry->ObjName).ToString();
					outerRef = outerEntry->ObjOuter;
				}
			}
			if ((group.IsNone() && !entryGroup.empty()) || (!group.IsNone() && group != entryGroup))
				continue;
		}

		if (isClass)
		{
			if (entry.ObjClass == 0)
			{
				return (int)index + 1;
			}
			else if (entry.ObjClass < 0)
			{
				auto classImport = GetImportEntry(entry.ObjClass);
				if (classImport && className == GetName(classImport->ObjName))
					return (int)index + 1;
			}
			else
			{
				// An export reference is its index plus one
				auto classExport = GetExportEntry(entry.ObjClass);
				if (classExport && className == GetName(classExport->ObjName))
					return (int)index + 1;
			}
		}
		else if (entry.ObjClass != 0)
		{
			UClass* cls = UObject::Cast<UClass>(GetUObject(entry.ObjClass));
			while (cls)
			{
				if (className == cls->Name)
					return (int)index + 1;
				cls = static_cast<UClass*>(cls->BaseStruct);
			}
		}
	}

	return 0;
}

const NameString& Package::GetName(int index) const
{
	if (index >= 0 && (size_t)index < NameTable.size())
		return NameTable[index].Name;
	else
		Exception::Throw("Name index out of bounds!: " + Name.ToString());
}

ExportTableEntry* Package::GetExportEntry(int objref)
{
	if (objref == 0)
		return nullptr;
	else if (objref < 0)
		Exception::Throw("Expected an export table entry: " + Name.ToString());

	int index = objref - 1;
	if ((size_t)index >= ExportTable.size())
		Exception::Throw("Export table entry out of bounds!: " + Name.ToString());

	return ExportTable.data() + index;
}

ImportTableEntry* Package::GetImportEntry(int objref)
{
	if (objref == 0)
		return nullptr;
	else if (objref > 0)
		Exception::Throw("Expected an import table entry: " + Name.ToString());

	int index = -objref - 1;
	if ((size_t)index >= ImportTable.size())
		Exception::Throw("Import table entry out of bounds!: " + Name.ToString());

	return ImportTable.data() + index;
}

void Package::ReadTables()
{
	auto stream = Packages->GetStream(this);
	stream->Seek(0);

	uint32_t signature = stream->ReadInt32();
	if (signature != 0x9E2A83C1)
		Exception::Throw("Not an unreal package file: " + Name.ToString());

	Version = stream->ReadInt16();
	LicenseeMode = stream->ReadInt16();

	// A version above the game's own is read as its own, as the original's
	// linker reads it, which refuses none but one under 60 -- Core's
	// OldVersion question, its reader then running past the file's end, an
	// assertion (Core.dll 0x10160000) --; DXEngine stops at such a one here
	if (Version < 60)
		Exception::Throw("Unsupported unreal package version: " + Name.ToString());

	Flags = (PackageFlags)stream->ReadUInt32();

	uint32_t nameCount = stream->ReadInt32();
	uint32_t nameOffset = stream->ReadInt32();

	uint32_t exportCount = stream->ReadInt32();
	uint32_t exportOffset = stream->ReadInt32();

	uint32_t importCount = stream->ReadInt32();
	uint32_t importOffset = stream->ReadInt32();

	if (Version < 68)
	{
		// An older file has a heritage of GUIDs, the last its own, and one
		// generation: its counts now.
		uint32_t heritageCount = stream->ReadInt32();
		uint32_t heritageOffset = stream->ReadInt32();
		if (heritageCount > 0)
		{
			stream->Seek(heritageOffset + (heritageCount - 1) * 16);
			stream->ReadBytes(Guid, 16);
		}
		Generation generation;
		generation.ExportCount = (int)exportCount;
		generation.NameCount = (int)nameCount;
		Generations.push_back(generation);
	}
	else
	{
		stream->ReadBytes(Guid, 16);
		uint32_t generationCount = stream->ReadInt32();
		for (uint32_t i = 0; i < generationCount; i++)
		{
			Generation generation;
			generation.ExportCount = stream->ReadInt32();
			generation.NameCount = stream->ReadInt32();
			Generations.push_back(generation);
		}
	}

	// A name outside the context (LoadContext) -- a map's editor-only one,
	// which only the editor's exports use -- is None and makes no name, as
	// the original's linker maps it (Core.dll 0x10160543, before the FName
	// at 0x10160553): one of these was a name's first spelling here
	// (02_NYC_BatteryPark's BArricade, which 06_HongKong_WanChai_Garage
	// spells Barricade). A save's names are all made, as its exports are.
	stream->Seek(nameOffset);
	for (uint32_t i = 0; i < nameCount; i++)
	{
		NameTableEntry entry;
		std::string text = stream->ReadString();
		entry.Flags = stream->ReadInt32();
		bool made = IsSaveFile() || AnyFlags((ObjectFlags)entry.Flags, LoadContext());
		if (made)
			entry.Name = NameString(text);
		NameTable.push_back(entry);
		if (made)
			NameHash[entry.Name] = i;
	}

	stream->Seek(exportOffset);
	for (uint32_t i = 0; i < exportCount; i++)
	{
		ExportTableEntry entry;
		entry.ObjClass = stream->ReadIndex();
		entry.ObjBase = stream->ReadIndex();
		entry.ObjOuter = stream->ReadInt32();
		entry.ObjName = stream->ReadIndex();
		entry.ObjFlags = (ObjectFlags)stream->ReadInt32();
		entry.ObjSize = stream->ReadIndex();
		entry.ObjOffset = (entry.ObjSize > 0) ? stream->ReadIndex() : -1;
		ExportTable.push_back(entry);
	}

	stream->Seek(importOffset);
	for (uint32_t i = 0; i < importCount; i++)
	{
		ImportTableEntry entry;
		entry.ClassPackage = stream->ReadIndex();
		entry.ClassName = stream->ReadIndex();
		entry.ObjOuter = stream->ReadInt32();
		entry.ObjName = stream->ReadIndex();
		ImportTable.push_back(entry);
	}

	FileExportCount = (int)exportCount;
	FileNameCount = (int)nameCount;
}

std::unique_ptr<ObjectStream> Package::OpenObjectStream(int index, const NameString& name, UClass* base)
{
	const auto& entry = ExportTable[index];
	if (entry.ObjSize > 0)
	{
		std::unique_ptr<uint64_t[]> buffer(new uint64_t[(entry.ObjSize + 7) / 8]);
		auto stream = Packages->GetStream(this);
		stream->Seek(entry.ObjOffset);
		stream->ReadBytes(buffer.get(), entry.ObjSize);
		return std::make_unique<ObjectStream>(this, std::move(buffer), entry.ObjOffset, entry.ObjSize, name, base);
	}
	else
	{
		return std::make_unique<ObjectStream>(this, std::unique_ptr<uint64_t[]>(), 0, 0, name, base);
	}
}

std::string Package::GetExportName(int objref)
{
	// objref counts from 1, as GetExportEntry's does; the walk up the outers
	// only reads the table
	if (objref <= 0 || (size_t)objref > ExportTable.size())
		return "None";

	const ExportTableEntry* entry = &ExportTable[objref - 1];
	std::string objname = NameTable[entry->ObjName].Name.ToString();

	while (entry->ObjOuter > 0 && (size_t)entry->ObjOuter <= ExportTable.size())
	{
		entry = &ExportTable[entry->ObjOuter - 1];
		objname = NameTable[entry->ObjName].Name.ToString() + '.' + objname;
	}

	objname = Name.ToString() + '.' + objname;
	return objname;
}

Array<NameString> Package::GetImportedPackages() const
{
	Array<NameString> result;
	for (const ImportTableEntry& entry : ImportTable)
	{
		if (entry.ObjOuter == 0 && GetName(entry.ClassName) == "Package")
			result.push_back(GetName(entry.ObjName));
	}
	return result;
}

std::string Package::GetGuidString() const
{
	char text[33];
	uint32_t d[4];
	for (int i = 0; i < 4; i++)
		d[i] = Guid[i * 4] | (Guid[i * 4 + 1] << 8) | (Guid[i * 4 + 2] << 16) | ((uint32_t)Guid[i * 4 + 3] << 24);
	snprintf(text, sizeof(text), "%08X%08X%08X%08X", d[0], d[1], d[2], d[3]);
	return text;
}

void Package::OnGCDestroy()
{
	Packages->UnloadPackage(this);
}
