
#include "Precomp.h"
#include "NativesAudit.h"
#include "GameFolder.h"
#include "Package/Package.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/UFunction.h"
#include "Utils/File.h"
#include "Utils/StrTools.h"
#include "VM/NativeFunc.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

// Whether a call of the function finds a handler, as CallNative looks for
// one: by its number in the table, else by its own name and its class's (or
// state's), the struct whose function it is (UStruct::Load).
static bool HasNative(const UFunction* func)
{
	if (func->NativeFuncIndex != 0)
	{
		size_t index = (size_t)func->NativeFuncIndex;
		return index < NativeFunctions::NativeByIndex.size() && NativeFunctions::NativeByIndex[index];
	}
	if (!func->NativeStruct)
		return false;
	auto it = NativeFunctions::NativeByName.find({ func->Name, func->NativeStruct->Name });
	return it != NativeFunctions::NativeByName.end() && it->second;
}

// A name or a message as one line of the answer: a control character in a
// package's names or a file's (a line's end among them) printed as '?', so
// that no text of a package's makes a line of its own
static std::string OneLine(std::string text)
{
	for (char& c : text)
	{
		if ((unsigned char)c < 0x20 || c == 0x7f)
			c = '?';
	}
	return text;
}

// An error's first line: the engine's own errors carry where they were
// thrown on the lines after it
static std::string FirstLine(const std::string& text)
{
	return text.substr(0, text.find('\n'));
}

// Whether text, folded, names a class of the package: its folded name and a
// '.', no letter, digit or '_' before it (an ini's "Root=Pkg.Class")
static bool NamesClassOf(const std::string& text, const std::string& package)
{
	const std::string token = package + ".";
	for (size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + 1))
	{
		unsigned char before = at ? (unsigned char)text[at - 1] : (unsigned char)'=';
		if (!std::isalnum(before) && before != '_')
			return true;
	}
	return false;
}

int NativesAudit::Run(const std::string& folder)
{
	try
	{
		std::error_code ec;
		std::filesystem::path dir = std::filesystem::absolute(convert_path_separators(folder), ec);
		if (ec || !std::filesystem::is_directory(dir, ec))
			throw std::runtime_error("--audit-natives: \"" + folder + "\" is no folder");
		dir = Directory::resolveCase(dir.lexically_normal());
		if (dir.filename().empty())
			dir = dir.parent_path();

		GameFolderSelection::UpdateList();
		if (GameFolderSelection::Games.empty())
			throw std::runtime_error(GameFolderSelection::NotFound);
		GameLaunchInfo info = GameFolderSelection::GetLaunchInfo(0);
		info.auditFolder = dir.string();
		// The game's packages and the natives registered, as a run has them;
		// nothing written, and no level, window or sound
		auto packages = std::make_unique<PackageManager>(info);

		// The folder's packages, in their names' order
		std::vector<std::filesystem::path> files;
		std::filesystem::directory_iterator entries(dir, ec);
		for (std::filesystem::directory_iterator end; !ec && entries != end; entries.increment(ec))
		{
			if (entries->is_regular_file(ec) && StrTools::equals_ignore_case(entries->path().extension().string(), ".u"))
				files.push_back(entries->path());
		}
		std::sort(files.begin(), files.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
			return NameString::Folded(a.filename().string()) < NameString::Folded(b.filename().string());
		});

		// The packages a script of the mod's can reach a native of: those that
		// take a game package's place, those another of the folder's builds on
		// (imports), and those an ini of the folder names a class of. A package
		// none of them holds for is the mod's own DLLs' alone to load, which
		// no engine but the original runs: its natives stop no game here, and
		// are told apart. Read from each file's tables alone, so that a
		// package whose imports are not there counts as one that builds on
		// them
		std::set<std::string> reached;
		{
			std::error_code ignored;
			std::filesystem::directory_iterator own(packages->GetSystemFolderPath(), ignored);
			for (std::filesystem::directory_iterator end; !ignored && own != end; own.increment(ignored))
			{
				if (StrTools::equals_ignore_case(own->path().extension().string(), ".u"))
					reached.insert(NameString::Folded(own->path().stem().string()));
			}
			std::string inis;
			std::filesystem::directory_iterator texts(dir, ignored);
			for (std::filesystem::directory_iterator end; !ignored && texts != end; texts.increment(ignored))
			{
				if (!texts->is_regular_file(ignored) || !StrTools::equals_ignore_case(texts->path().extension().string(), ".ini"))
					continue;
				try
				{
					inis += NameString::Folded(File::read_all_text(texts->path().string())) + "\n";
				}
				catch (const std::exception&)
				{
				}
			}
			for (const std::filesystem::path& file : files)
			{
				NameString name(file.stem().string());
				if (NamesClassOf(inis, NameString::Folded(name.ToString())))
					reached.insert(NameString::Folded(name.ToString()));
				try
				{
					Package* tables = GC::Alloc<Package>(packages.get(), name, file.string());
					for (const NameString& imported : tables->GetImportedPackages())
					{
						if (imported != name)
							reached.insert(NameString::Folded(imported.ToString()));
					}
				}
				catch (const std::exception&)
				{
					// Said below, where the package is read for its natives
				}
			}
		}

		std::cout << "native-audit: " << OneLine(dir.string()) << std::endl;
		int declared = 0;
		std::set<std::string> lacks;
		std::map<std::string, int> apart;
		for (const std::filesystem::path& file : files)
		{
			NameString name(file.stem().string());
			// The editor's own package, which the game's System has too: its
			// natives, the brush builders', are Editor.dll's, called by the
			// editor alone and by no game, and the engine has none of them.
			// A mod that ships the package has no native code of its own by it
			if (name == "Editor")
				continue;
			try
			{
				Package* package = packages->GetPackage(name);
				// Its classes loaded, and with each its functions and its
				// states' (UStruct::Load): a package of no class (textures,
				// sounds) loads nothing more
				int count = package->GetExportCount();
				for (int objref = 1; objref <= count; objref++)
				{
					if (package->GetExportEntry(objref)->ObjClass == 0)
						package->GetUObject(objref);
				}
				for (int index = 0; index < count; index++)
				{
					UFunction* func = UObject::TryCast<UFunction>(package->GetLoadedExport(index));
					if (!func || !AllFlags(func->FuncFlags, FunctionFlags::Native))
						continue;
					if (HasNative(func))
					{
						declared++;
						continue;
					}
					if (reached.find(NameString::Folded(name.ToString())) == reached.end())
					{
						apart[package->GetPackageName().ToString()]++;
						continue;
					}
					declared++;
					std::string line = package->GetPackageName().ToString() + "." +
						(func->NativeStruct ? func->NativeStruct->Name.ToString() : std::string("?")) + "." + func->Name.ToString();
					if (func->NativeFuncIndex != 0)
						line += " (" + std::to_string(func->NativeFuncIndex) + ")";
					lacks.insert(OneLine(line));
				}
			}
			catch (const std::exception& e)
			{
				// A damaged package, or one whose imports are not there: said,
				// and the others read
				std::cout << "damaged: " << OneLine(file.filename().string()) << ": " << OneLine(FirstLine(e.what())) << std::endl;
			}
		}
		for (const std::string& line : lacks)
			std::cout << "lacks: " << line << "\n";
		for (const auto& [package, count] : apart)
			std::cout << "apart: " << OneLine(package) << ": " << count << " the engine lacks, in a package no script of the folder's reaches\n";
		int lacking = (int)lacks.size();
		std::cout << "natives: " << declared << " declared, " << declared - lacking << " the engine has, " << lacking
			<< " it lacks" << std::endl;
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "DXEngine error: " << e.what() << std::endl;
		return 1;
	}
}
