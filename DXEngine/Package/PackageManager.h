#pragma once

#include "GC/GC.h"
#include "Package.h"
#include "IniFile.h"
#include "GameFolder.h"
#include <list>
#include <filesystem>

namespace fs = std::filesystem;

class PackageStream;
class UObject;
class UClass;

struct IntObject
{
	NameString Name;
	NameString Class;
	NameString MetaClass;
	std::string Description;
};

struct NativeClass
{
	NameString Package;
	NameString Name;
	NameString Base;
	// The C++ class's own flags (ClassFlags), which one its package's file
	// lacks takes, as the original's class has them from C++: Sound's and
	// Music's CLASS_SafeReplace (Engine.dll's classes' constructors)
	uint32_t ClsFlags = 0;
	std::function<UObject* (const NameString& name, UClass* cls, ObjectFlags flags)> CreateFunc;
};

class PackageManager
{
public:
	PackageManager(const GameLaunchInfo& launchInfo);

	fs::path GetRootFolderPath() const { return gameRootFolderPath; }
	fs::path GetSystemFolderPath() const { return gameSystemFolderPath; }
	fs::path GetSaveFolderPath() const { return gameSaveFolderPath; }
	fs::path GetCacheFolderPath() const { return gameCacheFolderPath; }

	Package* GetPackage(const NameString& name);
	// Whether a package of that name is on the paths, and whether it is a map.
	bool HasPackage(const NameString& name) const { return packageFilenames.find(NameString::Folded(name.ToString())) != packageFilenames.end(); }
	bool IsMapPackage(const NameString& name) const;
	void ScanSaveInfos();
	Package* GetSaveInfoPackage(const NameString& saveFolderName);
	void RemoveSaveInfoPackage(const NameString& saveFolderName);
	std::map<NameString, Package*> GetSaveInfoPackages() const { return saveInfos; };

	Package* LoadMap(const std::string& path);
	// Whether the file LoadMap opens for that map is there.
	bool MapExists(const std::string& path) const;
	// The file LoadMap opens for that map, there or not.
	std::string MapFilePath(const std::string& path) const { return FindMapFile(path).string(); }
	// Whether a file's first four bytes are there and not the package tag,
	// as the original's linker reads them before anything else (Core.dll
	// 0x10160000). A file shorter than that is not: the original's reader
	// stops the game there, as the package's own read stops DXEngine.
	static bool LacksPackageTag(const std::string& file);
	// The imports of a map's or a save's file checked before its load, as the
	// original's linker checks each as it opens the file (bugs.md PKG.38):
	// false with the linker's error where one is not found
	bool VerifyFileImports(const std::string& file, std::string& error, std::string& failedPackage);
	// A map from a file of any name, as the package of the name given: a
	// download kept in the cache.
	Package* LoadMapFile(const NameString& name, const std::string& path);
	void UnloadPackage(Package* package);

	// A package's file for a net game, as Core's appFindPackageFile finds
	// one: the search paths' by name, else a download kept in the cache by
	// its GUID, whose date is then brought up to now; "" for none.
	std::string FindPackageFile(const NameString& name, const std::string& guid);
	// Where a download of that GUID is looked for: the cache, the GUID, CacheExt.
	std::string GetCachedPackagePath(const std::string& guid) const; // none for a GUID that is not 32 hex digits
	// Where a download of that GUID is written: the cache, the GUID, .uxx
	// whatever CacheExt says, as the original's file channel moves it there
	// (UFileChannel::Destroy, Engine.dll 0x10400a40).
	std::string GetCacheDownloadPath(const std::string& guid) const;
	static bool IsPackageGuid(const std::string& guid);
	// A package file's GUID from its header alone, as Package gives it.
	static std::string ReadPackageGuid(const std::string& path);
	bool IsPackageLoaded(const NameString& name) const;
	// A package of a net game found under its name in this file -- a download
	// in the cache --, until RestorePackageFiles.
	void UsePackageFile(const NameString& name, const std::string& path);
	// Each name's own file again, but a package loaded from its download,
	// which stays until ReleaseNetPackages lets it go.
	void RestorePackageFiles();
	// The packages loaded from downloads let go, but those named in `keep`,
	// and their names' own files back, as the original's map load collects
	// what the new level does not use: another server's package of such a
	// name then loads in their place.
	void ReleaseNetPackages(const Array<NameString>& keep);
	// The cache's downloads left unfinished, and those unused for more than
	// PurgeCacheDays (Core's appCleanFileCache).
	void CleanFileCache();

	void CloseStreams();

	// The collector's roots here: the packages of `packages`, their code
	// kept as all code is, and every export of a save info. A map, a save
	// and a download are not among them: what their levels reach stays.
	void MarkRoots(GCMarker& marker);
	// Objects made but not loaded yet: no collection may run then.
	bool HasPendingLoads() const { return !delayLoads.empty(); }
	// The open files of dying packages closed (GC::IsDying).
	void PurgeDying();
	// Whether a package is one of `packages` or a save info.
	bool IsRegisteredPackage(const Package* package) const;

	Package* LoadSaveFile(const std::string& path);
	// The file LoadSaveFile opens for that path; "" where there is none.
	std::string SaveFilePath(const std::string& path) const;
	std::string FindCurrentSave(const std::string& mapName) const;

	std::shared_ptr<PackageStream> GetStream(Package* package);

	UClass* FindClass(const NameString& name);
	// A class by its package and name, or by its name alone in any package
	// loaded, as the original's console finds one (StaticFindObject with
	// ANY_PACKAGE): the game's menus name DeusExMPGame, not DeusEx.DeusExMPGame.
	UClass* FindClassAnyPackage(const NameString& name);
	// An object of the class by a path (Package.Group.Name, its package loaded
	// as need be) or by a bare name among the loaded objects, as the original's
	// StaticFindObject in any package finds it; the classes by FindClassAnyPackage
	UObject* FindObjectAnyPackage(UClass* cls, const std::string& path);

	std::string GetMapExtension() const { return mapExtension; }
	std::string GetSaveExtension() const { return saveExtension; }

	std::unique_ptr<IniFile> GetIniFile(NameString iniName);
	std::unique_ptr<IniFile> GetUserIniFile();
	std::unique_ptr<IniFile> GetSystemIniFile();
	Array<std::string> GetIniKeysFromSection(NameString iniName, const NameString& sectionName);
	// Each key of the section and its value (IniFile::GetKeyValues), the key
	// with no name among them
	Array<std::pair<std::string, std::string>> GetIniKeyValues(NameString iniName, const NameString& sectionName);
	std::string GetIniValue(NameString iniName, const NameString& sectionName, const NameString& keyName, std::string default_value = "");
	bool FindIniValue(NameString iniName, const NameString& sectionName, const NameString& keyName, std::string& value);
	Array<std::string> GetIniValues(NameString iniName, const NameString& sectionName, const NameString& keyName, Array<std::string> default_values = {});
	std::string GetDefaultIniValue(const NameString& sectionName, const NameString& keyName, std::string default_value = "");
	Array<std::string> GetDefaultIniValues(const NameString& sectionName, const NameString& keyName, Array<std::string> default_values = {});
	std::string GetDefUserIniValue(const NameString& sectionName, const NameString& keyName, std::string default_value = "");
	Array<std::string> GetDefUserIniValues(const NameString& sectionName, const NameString& keyName, Array<std::string> default_values = {});
	// The section copied key by key into the live system or user ini from
	// Default.ini or DefUser.ini, as the original's ResetConfig copies a
	// class's; keys the default file lacks keep their values.
	void ResetIniSection(NameString iniName, const NameString& sectionName);
	void SetIniValue(NameString iniName, const NameString& sectionName, const NameString& keyName, const std::string& newValue);
	void SetIniValues(NameString iniName, const NameString& sectionName, const NameString& keyName, const Array<std::string>& newValues);
	// Every ini written back, as the engine shuts down
	void SaveAllIniFiles();
	// Each ini changed since it was last written written now, where
	// SaveAllIniFiles writes it, the rest left: the original's config
	// cache's Flush(0) (GOG DeusEx.exe 0x10915250), which every SaveConfig
	// ends with (Core.dll 0x10151540), the files kept for the rest of the
	// run (bugs.md ENG.27)
	void FlushIniFiles();

	// A key's text from the first of its package's .int files to give it
	// (LoadIntFiles), "" when none does; FindLocalized says whether one does.
	std::string Localize(NameString packageName, const NameString& sectionName, const NameString& keyName);
	bool FindLocalized(NameString packageName, const NameString& sectionName, const NameString& keyName, std::string& value);

	Array<IntObject>& GetIntObjects(const NameString& metaclass);
	const Array<std::string>& GetMaps() const { return maps; }
	const Array<std::string>& GetMapFolders() const { return mapFolders; }

	bool MissingDXESystemIni() const { return missing_dxe_system_ini; }

	Package* GetTransientPackage() { return GetPackage("Transient"); }
	Package* CreateEmptyPackage(const NameString& name);

	// An object of cls made by its nearest native class, with none of a
	// package's bookkeeping; null when none of its classes is native.
	UObject* CreateNativeObject(const NameString& name, UClass* cls, ObjectFlags flags);

private:
	void CreateTransientPackage();
	// The file of a map's name or path: a map folder's, by the map list's
	// spelling, else the path under Maps.
	fs::path FindMapFile(const std::string& path) const;

	std::unique_ptr<IniFile>& LoadIniFile(NameString iniName);
	std::unique_ptr<IniFile>& LoadUserIniFile();
	std::unique_ptr<IniFile>& LoadSystemIniFile();
	void LoadEngineIniFiles();
	void LoadFileExtensions();
	void LoadIntFiles();
	std::map<NameString, std::string> ParseIntPublicValue(const std::string& value);

	void ScanForMaps();

	void ScanFolder(const std::string& packagedir, const std::string& search);
	void ScanPaths();

	void DelayLoadNow();
	void RegisterFunctions();

	void RegisterNativeClasses();

	template<typename T>
	void RegisterNativeClass(const NameString& packageName, const NameString& className, const NameString& baseClass = {}, uint32_t clsFlags = 0);

	std::map<NameString, NativeClass> NativeClasses;

	Array<UObject*> delayLoads;
	int delayLoadActive = 0;

	// Each package's file by its name's Folded text: a file on the paths
	// makes no name, so a package is spelt as the packages that use it spell
	// it, as the original's (bugs.md VM.25)
	std::map<std::string, std::string> packageFilenames;
	// What UsePackageFile replaced: a name's file before, "" for none.
	std::map<NameString, std::string> replacedPackageFilenames;
	std::map<NameString, GCRoot<Package>> packages;
	std::map<NameString, std::unique_ptr<IniFile>> iniFiles;
	// Each package's .int files, in the order of the folders they are in, by
	// its name's Folded text: a .int makes no name.
	std::map<std::string, std::vector<std::unique_ptr<IniFile>>> intFiles;
	// Where a .int is looked for, in order: the paths' folders as they name
	// them, System among them where they name it, else last.
	Array<fs::path> intFolders;
	std::map<NameString, Package*> saveInfos;

	std::map<NameString, Array<IntObject>> IntObjects;

	std::unique_ptr<IniFile> defaultIniFile;  // Holds Default.ini
	std::unique_ptr<IniFile> defaultUserFile; // Holds DefUser.ini

	Array<std::string> mapFolders;
	Array<std::string> maps;

	// So that we don't have to build fs::path()s all over the place
	fs::path gameRootFolderPath;
	fs::path gameSystemFolderPath;
	fs::path gameSaveFolderPath;
	fs::path gameCacheFolderPath;

	std::string mapExtension;
	std::string saveExtension;

	bool missing_dxe_system_ini = false;

	// One ini written back where SaveAllIniFiles writes it: the system and
	// user inis to --ini's and --userini's files, else to the engine's
	// DXE- ones beside the game's, a class's own file to its own path
	void SaveIniFile(const NameString& name, IniFile& file);

	struct OpenStream
	{
		Package* Pkg = nullptr;
		std::string Path; // the package's file: a package whose construction failed leaves an entry its address can be given again
		std::shared_ptr<PackageStream> Stream;
	};

	std::list<OpenStream> openStreams;

	GameLaunchInfo launchInfo;

	friend class Package;
	friend struct SetDelayLoadActive;
};

struct SetDelayLoadActive
{
	SetDelayLoadActive(PackageManager* p) : p(p) { p->delayLoadActive++; }
	~SetDelayLoadActive() { p->delayLoadActive--; }
	PackageManager* p;
};
