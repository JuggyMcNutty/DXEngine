#pragma once

#include "NameString.h"
#include <map>

class IniKey
{
public:
	IniKey() = default;
	IniKey(const std::string& _name, const uint32_t _hash);
	IniKey(const IniKey& other);

	uint32_t GetHash() const;
	const std::string& GetName() const;
	const Array<std::string>& GetValues() const;
	bool GetIndexed() const;

	std::string GetValue(const int index = 0) const;

	int SetValue(const std::string& newValue, const int index = 0);
	int SetValues(const Array<std::string>& newValues);

	int SetIndexed(bool newIndexed);

private:
	std::string name;
	uint32_t hash;
	Array<std::string> values;
	bool indexed = false;
};

class IniSection
{
public:
	IniSection() = default;
	IniSection(const std::string& _name, const uint32_t _hash);
	IniSection(const IniSection& other);

	uint32_t GetHash() const;
	const std::string& GetName() const;
	const Array<IniKey>& GetKeys() const;

	// The value on the key's line `index` -- 0 its first, 1 its second, for
	// a key given more than once (UpdateFile's) -- an empty one, or none,
	// giving defaultValue.
	std::string GetValue(const NameString& keyName, const std::string& defaultValue, const int index) const;
	Array<std::string> GetValues(const NameString& keyName, const Array<std::string>& defaultValues = {}) const;
	// Whether the key is there, its value in `value` -- an empty one too;
	// of a key given twice or more, the last value.
	bool FindValue(const NameString& keyName, std::string& value) const;

	bool SetValue(const NameString& keyName, const std::string& newValue, const int index = 0, const bool indexed = false);
	bool SetValues(const NameString& keyName, const Array<std::string>& newValues, const bool indexed = false);
	// A value of the key with no name (a line "=value"), added after its
	// others, as the reader adds a key's: kept and written back, as the
	// original's config cache keeps it; no lookup names it
	void AddNamelessValue(const std::string& value);

	bool HasNewKey() const { return hasNewKey; }
	void SetHasNewKey(bool value) { hasNewKey = value; }

private:
	std::string name;
	uint32_t hash;
	Array<IniKey> keys;
	bool hasNewKey = false; // True when SetValue[s]() adds a new key to the Section.
};

class IniFile
{
public:
	IniFile() = default;
	IniFile(const std::string& filename);
	IniFile(const IniFile& other);

	// A text file's text as the engine's UTF-8, as the original's
	// appLoadFileToString reads one (Core.dll 0x101488c0): a file of even size
	// beginning FF FE UTF-16 little-endian, FE FF big-endian, any other a byte
	// a character, to the first NUL -- an ini, an exec's macro file
	static std::string ReadGameText(const std::string& filename);

	bool IsModified() const { return isModified; }
	// Where SaveTo() writes: for a file not there yet, begun empty.
	void SetPath(const std::string& filename) { ini_file_path = filename; }
	const std::string& GetPath() const { return ini_file_path; }

	// The section's keys as the file spells them
	Array<std::string> GetKeys(const NameString& sectionName) const;
	// The section's keys and the value each is read as, of a key given twice
	// or more the last, in the order of their first lines, the key with no
	// name (a line "=value") among them by its empty name, as the original's
	// FURL::LoadURLConfig adds a section's lines to a URL (Engine.dll
	// 0x103f9bc0), each in place of one of its name before it
	Array<std::pair<std::string, std::string>> GetKeyValues(const NameString& sectionName) const;
	// The key's value, of a key given twice or more the last, as the
	// original's config cache reads it; an empty one, or none, gives
	// defaultValue.
	std::string GetValue(const NameString& sectionName, const NameString& keyName, const std::string& defaultValue = "") const;
	Array<std::string> GetValues(const NameString& sectionName, const NameString& keyName, const Array<std::string>& defaultValues = {}) const;
	// Whether the key is there, its value in `value`, as the original's
	// config cache finds it: an empty value is one (GetValue takes it for
	// missing), and of a key given twice or more the last one counts.
	bool FindValue(const NameString& sectionName, const NameString& keyName, std::string& value) const;

	// Sets the value GetValue reads -- of a key given twice or more the last,
	// as the original's config cache sets it -- or adds the key.
	void SetValue(const NameString& sectionName, const NameString& keyName, const std::string& newValue);
	void SetValues(const NameString& sectionName, const NameString& keyName, const Array<std::string>& newValues);

	// Saves values to the ini file the said values are loaded from
	void SaveTo();
	// Saves values to a specified file
	void SaveTo(const std::string& filename);

	// The file updated from what is in memory, its comments and order kept
	// (UpdateFile(filename))
	void UpdateFile();
	// Updates values in a specified file
	void UpdateFile(const std::string& filename);

	// Updates the file with the changed values if it exists, otherwise saves the values to a new file instead
	void UpdateIfExists(const std::string& filename);

private:
	bool ReadLine(const std::string& text, size_t& pos, std::string& line);

	// Adds a new section, or gets the existing section
	IniSection& AddUniqueSection(const std::string& sectionName);
	const IniSection* FindSection(const std::string& sectionName) const;

	bool isModified = false;
	std::string ini_file_path;
	Array<IniSection> sections;
};