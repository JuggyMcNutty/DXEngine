#pragma once

#include "Package.h"

class NameString;
class UObject;
class PackageStreamWriter;

class PackageWriter
{
public:
	PackageWriter(Package* package);
	void Save(UObject* packageObject, std::string filename = {});

	// Conformed to another file of the package, as the original's
	// SavePackage conforms to a linker (Core.dll 0x10155980): the other's
	// names first, in its order; each of its exports kept at its index by the
	// object of that full name, or an empty slot no load makes; its GUID and
	// its generations, this save one more
	void SetConform(Package* conform) { Conform = conform; }

private:
	void ConformTables();
	std::string ExportFullName(Package* package, int index);
	std::string ObjectFullName(UObject* obj);

	void WriteHeader(PackageStreamWriter* stream);
	void WriteObjects(UObject* packageObject, PackageStreamWriter* stream);
	void WriteNameTable(PackageStreamWriter* stream);
	void WriteExportTable(PackageStreamWriter* stream);
	void WriteImportTable(PackageStreamWriter* stream);

	int GetVersion() const;
	int GetNameIndex(NameString name);
	int GetObjectReference(UObject* obj);
	int GetPackageReference(NameString packageName);

	Package* Source = nullptr;
	Array<NameTableEntry> NameTable;
	Array<ExportTableEntry> ExportTable;
	Array<ImportTableEntry> ImportTable;

	Array<UObject*> ExportObjects;
	// The exports in the order they were made, which the objects are written
	// in: a conformed slot is made when its object is first reached
	Array<int> ExportOrder;

	Package* Conform = nullptr;
	// A conformed slot's object, by the reference its slot has
	std::map<UObject*, int> Reserved;

	uint32_t NameOffset = 0;
	uint32_t ExportOffset = 0;
	uint32_t ImportOffset = 0;

	std::map<NameString, int> NameHash;
	std::map<UObject*, int> ObjRefHash;
	std::map<NameString, int> PackageReferences;

	friend class PackageStreamWriter;
};
