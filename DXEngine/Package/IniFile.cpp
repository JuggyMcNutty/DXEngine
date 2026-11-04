
#include "Precomp.h"
#include "IniFile.h"
#include "Utils/File.h"
#include "Utils/Convert.h"
#include "MurmurHash3/MurmurHash3.h"
#include <algorithm>
#include <map>

static uint32_t HashIniString(const std::string& str)
{
	std::string strLower;
	strLower.resize(str.size());
	std::transform(str.begin(), str.end(), strLower.begin(), [](unsigned char c) { return tolower(c); });

	uint32_t hash = 0;
	MurmurHash3_x86_32(strLower.c_str(), (int)strLower.length(), 0, &hash);

	return hash;
}

// A file's bytes as the engine's text, UTF-8, as the original's
// appLoadFileToString reads them (Core.dll 0x101488c0): a file of even size
// beginning FF FE UTF-16 little-endian, FE FF big-endian, the byte-order mark
// dropped; any other a byte a character, Latin-1.
static std::string ReadIniText(const std::string& filename)
{
	std::string bytes = File::read_all_text(filename);
	bool le = bytes.size() >= 2 && bytes.size() % 2 == 0 && (uint8_t)bytes[0] == 0xFF && (uint8_t)bytes[1] == 0xFE;
	bool be = bytes.size() >= 2 && bytes.size() % 2 == 0 && (uint8_t)bytes[0] == 0xFE && (uint8_t)bytes[1] == 0xFF;
	ScriptString text;
	if (le || be)
	{
		for (size_t i = 2; i + 1 < bytes.size(); i += 2)
		{
			uint8_t a = (uint8_t)bytes[i], b = (uint8_t)bytes[i + 1];
			text.push_back(le ? (char16_t)(a | (b << 8)) : (char16_t)((a << 8) | b));
		}
	}
	else
	{
		text = Latin1ToScript(bytes);
	}
	return ScriptToUTF8(text);
}

// The engine's text, UTF-8, as the original's appSaveStringToFile writes a
// file (Core.dll 0x10148c40): a byte a character when every one is below
// 0x100, else UTF-16 little-endian after the byte-order mark FF FE.
static void WriteIniText(const std::string& filename, const std::string& utf8)
{
	ScriptString text = UTF8ToScript(utf8);
	bool wide = false;
	for (char16_t c : text)
		wide = wide || c > 0xFF;
	if (!wide)
	{
		File::write_all_text(filename, ScriptToLatin1(text));
		return;
	}
	std::string bytes = "\xFF\xFE";
	bytes.reserve(2 + text.size() * 2);
	for (char16_t c : text)
	{
		bytes.push_back((char)(c & 0xFF));
		bytes.push_back((char)(c >> 8));
	}
	File::write_all_text(filename, bytes);
}

IniFile::IniFile(const std::string& filename)
{
	// A file that is not there yet (an --ini or --userini named before its
	// first write, a missing Default.ini) is empty, as the original's config
	// cache takes it, and written whole at the first write
	std::error_code ec;
	if (!fs::exists(filename, ec))
	{
		ini_file_path = filename;
		return;
	}
	std::string text = ReadIniText(filename);
	size_t pos = 0;
	std::string line;
	std::string sectionName;
	while (ReadLine(text, pos, line))
	{
		// Skip comments and empty lines
		// While ini files have ; as their comment indicator, some files in Wheel of Time use the good ol' // for some reason
		if (line.empty() || line[0] == ';' || (line[0] == '/' && line[1] == '/'))
			continue;

		if (line.size() >= 2 && line.front() == '[' && line.back() == ']')
		{
			sectionName = line.substr(1, line.size() - 2);
		}
		else
		{
			size_t equalpos = line.find('=');
			if (equalpos != std::string::npos)
			{
				std::string name = line.substr(0, equalpos);
				while (!name.empty() && (name.back() == ' ' || name.back() == '\t'))
					name.pop_back();

				std::string value;
				size_t valuepos = line.find_first_not_of(" \t", equalpos + 1);
				if (valuepos != std::string::npos)
					value = line.substr(valuepos);

				// A key above the first section header belongs to no section
				// and is dropped, as the original's config reader drops it:
				// kept, it was written back at the file's end under an empty
				// header, and once more at every write
				if (!name.empty() && !sectionName.empty())
				{
					int index = -1;
					bool indexed = false;
					/* This doesn't work
					size_t bracket = name.find('[');
					if (bracket != std::string::npos)
					{
						size_t rightBracket = name.find(']');
						if (rightBracket == std::string::npos)
							Exception::Throw("malformed INI array index");

						indexed = true;
						index = Convert::to_int32(name.substr(bracket + 1, rightBracket - bracket - 1));
						name = name.substr(0, bracket);
					}
					*/

					IniSection& section = AddUniqueSection(sectionName);
					section.SetValue(name, value, index, indexed);
				}
			}
		}
	}

	ini_file_path = filename;
}

IniFile::IniFile(const IniFile& other)
{
	ini_file_path = other.ini_file_path;
	sections = other.sections;
}

bool IniFile::ReadLine(const std::string& text, size_t& pos, std::string& line)
{
	// Skip whitespace
	while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n'))
		pos++;
	if (pos >= text.size())
		return false;

	size_t startpos = pos;

	// Read until end of line
	while (pos < text.size() && text[pos] != '\r' && text[pos] != '\n')
		pos++;

	size_t endpos = pos;

	// Skip whitespace at end of line
	while (endpos > startpos && (text[endpos - 1] == ' ' || text[endpos - 1] == '\t' || text[endpos - 1] == '\r' || text[endpos - 1] == '\n'))
		endpos--;

	// Read newline
	while (pos < text.size() && (text[pos] == '\r' || text[pos] == '\n'))
		pos++;

	line = text.substr(startpos, endpos - startpos);
	return true;
}

Array<NameString> IniFile::GetKeys(const NameString& sectionName) const
{
	Array<NameString> result;

	const IniSection* section = FindSection(sectionName.ToString());
	if (section != nullptr)
	{
		auto& keys = section->GetKeys();
		for (const IniKey& key : keys)
			result.push_back(key.GetName());
	}

	return result;
}

std::string IniFile::GetValue(const NameString& sectionName, const NameString& keyName, const std::string& defaultValue) const
{
	std::string value;
	if (!FindValue(sectionName, keyName, value) || value.empty())
		return defaultValue;
	return value;
}

bool IniFile::FindValue(const NameString& sectionName, const NameString& keyName, std::string& value) const
{
	const IniSection* section = FindSection(sectionName.ToString());
	return section && section->FindValue(keyName, value);
}

Array<std::string> IniFile::GetValues(const NameString& sectionName, const NameString& keyName, const Array<std::string>& defaultValues) const
{
	const IniSection* section = FindSection(sectionName.ToString());
	if (section == nullptr)
		return defaultValues;

	return section->GetValues(keyName, defaultValues);
}

void IniFile::SetValue(const NameString& sectionName, const NameString& keyName, const std::string& newValue)
{
	IniSection& section = AddUniqueSection(sectionName.ToString());
	int last = std::max((int)section.GetValues(keyName).size() - 1, 0);
	if (section.SetValue(keyName, newValue, last))
	{
		isModified = true;
	}
}

void IniFile::SetValues(const NameString& sectionName, const NameString& keyName, const Array<std::string>& newValues)
{
	IniSection& section = AddUniqueSection(sectionName.ToString());
	if (section.SetValues(keyName, newValues))
	{
		isModified = true;
	}
}

void IniFile::SaveTo()
{
	SaveTo(ini_file_path);
}

void IniFile::SaveTo(const std::string& filename)
{
	if (filename == ini_file_path && !isModified)
		return;

	std::string ini_text = "";

	// Each section, its keys and a blank line, each line ending in CR LF, as
	// the original's config cache writes an ini whole
	for (auto& section : sections)
	{
		// Section header (i.e. [Engine.Engine])
		std::string section_text = "[" + section.GetName() + "]\r\n";

		// key=value pairs
		auto& keys = section.GetKeys();
		for (auto& key : keys)
		{
			// a key can hold multiple values, like
			// Paths=path1
			// Paths=path2
			// and so on, hence this loop
			auto& values = key.GetValues();
			for (auto& value : values)
			{
				section_text += key.GetName() + "=" + value + "\r\n";
			}
		}
		section.SetHasNewKey(false); // New key in section won't be new anymore after saving.

		ini_text += section_text + "\r\n";
	}

	// Overwrite whatever is there
	WriteIniText(filename, ini_text);

	if (filename != ini_file_path)
		ini_file_path = filename;

	isModified = false;
}

void IniFile::UpdateFile()
{
	UpdateFile(ini_file_path);
}

// The file as it is, its comments and order kept: each value in memory that
// differs from its line is written into that line (a key given more than
// once, each occurrence its own value, in order); values the file has no
// line for are added after the last key line of their section (after the
// key's own last line when it has some), and sections it has not at its end.
// A key's name is taken as written, Name[0] included, as the file is read.
void IniFile::UpdateFile(const std::string& filename)
{
	if (filename == ini_file_path && !isModified)
		return;

	// A file not there yet -- a class's own config file before its first
	// SaveConfig, as a mod's -- is written whole, as the original makes it.
	if (!File::try_open_existing(filename))
	{
		SaveTo(filename);
		return;
	}

	Array<std::string> lines;
	{
		std::string text = ReadIniText(filename);
		size_t start = 0;
		while (start < text.size())
		{
			size_t end = text.find('\n', start);
			if (end == std::string::npos)
				end = text.size();
			lines.push_back(text.substr(start, end - start));
			start = end + 1;
		}
	}

	// A file written on Windows -- the game's own -- ends its lines with CR
	// LF: matched without the CR, and written back with it.
	bool crlf = false;
	for (std::string& l : lines)
	{
		if (!l.empty() && l.back() == '\r')
		{
			l.pop_back();
			crlf = true;
		}
	}

	struct SeenSection
	{
		const IniSection* Section = nullptr;
		size_t LastLine = 0; // its header, then its last key line
		std::map<uint32_t, int> Occurrences; // a key's lines so far
		std::map<uint32_t, size_t> KeyLastLine;
	};
	Array<SeenSection> seen;
	size_t currentIndex = (size_t)-1;

	auto trim = [](const std::string& str) {
		size_t start = str.find_first_not_of(" \t");
		if (start == std::string::npos)
			return std::string();
		size_t end = str.find_last_not_of(" \t");
		return str.substr(start, end - start + 1);
	};

	for (size_t i = 0; i < lines.size(); i++)
	{
		std::string line = trim(lines[i]);
		if (line.empty() || line[0] == ';' || (line.size() >= 2 && line[0] == '/' && line[1] == '/'))
			continue;

		if (line.size() >= 2 && line.front() == '[' && line.back() == ']')
		{
			// A section given twice in the file is one section in memory, its
			// keys' values in the file's order: the second header goes on
			// with the first's counts, so each line meets its own value
			const IniSection* section = FindSection(line.substr(1, line.size() - 2));
			currentIndex = (size_t)-1;
			for (size_t j = 0; section && j < seen.size(); j++)
			{
				if (seen[j].Section == section)
					currentIndex = j;
			}
			if (currentIndex == (size_t)-1)
			{
				SeenSection entry;
				entry.Section = section;
				seen.push_back(std::move(entry));
				currentIndex = seen.size() - 1;
			}
			seen[currentIndex].LastLine = i;
			continue;
		}

		if (currentIndex == (size_t)-1 || !seen[currentIndex].Section)
			continue;
		SeenSection& current = seen[currentIndex];

		size_t equalpos = line.find('=');
		if (equalpos == std::string::npos)
			continue;
		std::string name = line.substr(0, equalpos);
		while (!name.empty() && (name.back() == ' ' || name.back() == '\t'))
			name.pop_back();
		if (name.empty())
			continue;
		std::string valueInFile;
		size_t valuepos = line.find_first_not_of(" \t", equalpos + 1);
		if (valuepos != std::string::npos)
			valueInFile = line.substr(valuepos);

		uint32_t keyHash = HashIniString(name);
		int occurrence = current.Occurrences[keyHash]++;
		current.LastLine = i;
		current.KeyLastLine[keyHash] = i;

		for (const IniKey& key : current.Section->GetKeys())
		{
			if (key.GetHash() != keyHash)
				continue;
			if (occurrence < (int)key.GetValues().size())
			{
				const std::string& value = key.GetValues()[occurrence];
				if (value != valueInFile)
					lines[i] = name + "=" + value;
			}
			break;
		}
	}

	// What the file has no line for, added after its section's lines
	struct Insertion
	{
		size_t After;
		Array<std::string> Lines;
	};
	Array<Insertion> insertions;
	for (SeenSection& entry : seen)
	{
		if (!entry.Section)
			continue;
		for (const IniKey& key : entry.Section->GetKeys())
		{
			int inFile = entry.Occurrences.count(key.GetHash()) ? entry.Occurrences[key.GetHash()] : 0;
			const Array<std::string>& values = key.GetValues();
			if ((int)values.size() <= inFile)
				continue;
			Insertion insertion;
			insertion.After = inFile > 0 ? entry.KeyLastLine[key.GetHash()] : entry.LastLine;
			for (size_t v = inFile; v < values.size(); v++)
				insertion.Lines.push_back(key.GetName() + "=" + values[v]);
			insertions.push_back(std::move(insertion));
		}
	}
	// From the bottom up, so each one's line number still holds; those after
	// one line in their order
	std::stable_sort(insertions.begin(), insertions.end(), [](const Insertion& a, const Insertion& b) { return a.After > b.After; });
	for (size_t i = 0; i < insertions.size(); )
	{
		size_t after = insertions[i].After;
		Array<std::string> block;
		for (; i < insertions.size() && insertions[i].After == after; i++)
			block.insert(block.end(), insertions[i].Lines.begin(), insertions[i].Lines.end());
		lines.insert(lines.begin() + after + 1, block.begin(), block.end());
	}

	// Sections the file has not, at its end
	for (const IniSection& section : sections)
	{
		bool inFile = false;
		for (const SeenSection& entry : seen)
		{
			if (entry.Section == &section)
			{
				inFile = true;
				break;
			}
		}
		if (inFile || section.GetKeys().empty())
			continue;
		if (!lines.empty() && !lines.back().empty())
			lines.push_back("");
		lines.push_back("[" + section.GetName() + "]");
		for (const IniKey& key : section.GetKeys())
		{
			for (const std::string& value : key.GetValues())
				lines.push_back(key.GetName() + "=" + value);
		}
	}

	std::string final_text;
	for (const std::string& line : lines)
	{
		final_text += line;
		final_text += crlf ? "\r\n" : "\n";
	}
	WriteIniText(filename, final_text);

	if (filename != ini_file_path)
		ini_file_path = filename;

	isModified = false;
}

void IniFile::UpdateIfExists(const std::string& filename)
{
	if (File::try_open_existing(filename))
		UpdateFile(filename);
	else
		SaveTo(filename);
}

IniSection& IniFile::AddUniqueSection(const std::string& sectionName)
{
	uint32_t sectionHash = HashIniString(sectionName);
	for (auto& section : sections)
	{
		if (section.GetHash() == sectionHash)
			return section;
	}

	sections.push_back(IniSection(sectionName, sectionHash));
	return sections.back();
}

const IniSection* IniFile::FindSection(const std::string& sectionName) const
{
	uint32_t sectionHash = HashIniString(sectionName);
	for (auto& section : sections)
	{
		if (section.GetHash() == sectionHash)
			return &section;
	}

	return nullptr;
}

//====================================================================

IniSection::IniSection(const std::string& _name, const uint32_t _hash)
{
	name = _name;
	hash = _hash;
}

IniSection::IniSection(const IniSection& other)
{
	name = other.name;
	hash = other.hash;
	keys = other.keys;
}

const std::string& IniSection::GetName() const
{
	return name;
}

uint32_t IniSection::GetHash() const
{
	return hash;
}

const Array<IniKey>& IniSection::GetKeys() const
{
	return keys;
}

std::string IniSection::GetValue(const NameString& keyName, const std::string& defaultValue, const int index) const
{
	uint32_t keyHash = HashIniString(keyName.ToString());
	for (auto& key : keys)
	{
		if (key.GetHash() == keyHash)
		{
			std::string value = key.GetValue(index);
			if (value.size() == 0)
				break;

			return value;
		}
	}

	return defaultValue;
}

bool IniSection::FindValue(const NameString& keyName, std::string& value) const
{
	uint32_t keyHash = HashIniString(keyName.ToString());
	for (auto& key : keys)
	{
		if (key.GetHash() == keyHash)
		{
			if (key.GetValues().empty())
				return false;
			value = key.GetValues().back();
			return true;
		}
	}
	return false;
}

Array<std::string> IniSection::GetValues(const NameString& keyName, const Array<std::string>& defaultValues) const
{
	uint32_t keyHash = HashIniString(keyName.ToString());
	for (auto& key : keys)
	{
		if (key.GetHash() == keyHash)
			return key.GetValues();
	}

	return defaultValues;
}

bool IniSection::SetValue(const NameString& keyName, const std::string& newValue, const int index, const bool indexed)
{
	int result = -1;
	uint32_t keyHash = HashIniString(keyName.ToString());
	for (auto& key : keys)
	{
		if (key.GetHash() == keyHash)
		{
			result = key.SetValue(newValue, index);
			result |= key.SetIndexed(indexed);
			break;
		}
	}

	if (result < 0)
	{
		IniKey newKey(keyName.ToString(), keyHash);
		newKey.SetValue(newValue, index);
		newKey.SetIndexed(indexed);
		keys.push_back(std::move(newKey));
		hasNewKey = true;
		return true;
	}

	return result == 1;
}

bool IniSection::SetValues(const NameString& keyName, const Array<std::string>& newValues, const bool indexed)
{
	int result = -1;
	uint32_t keyHash = HashIniString(keyName.ToString());
	for (auto& key : keys)
	{
		if (key.GetHash() == keyHash)
		{
			result = key.SetValues(newValues);
			result |= key.SetIndexed(indexed);
			break;
		}
	}

	if (result < 0)
	{
		IniKey newKey(keyName.ToString(), keyHash);
		newKey.SetValues(newValues);
		newKey.SetIndexed(indexed);
		keys.push_back(std::move(newKey));
		hasNewKey = true;
		return true;
	}

	return result == 1;
}

//====================================================================

IniKey::IniKey(const std::string& _name, const uint32_t _hash)
{
	name = _name;
	hash = _hash;
}

IniKey::IniKey(const IniKey& other)
{
	name = other.name;
	hash = other.hash;
	values = other.values;
}

uint32_t IniKey::GetHash() const
{
	return hash;
}

const std::string& IniKey::GetName() const
{
	return name;
}

const Array<std::string>& IniKey::GetValues() const
{
	return values;
}

bool IniKey::GetIndexed() const
{
	return indexed;
}

std::string IniKey::GetValue(const int index) const
{
	std::string value = "";
	if ((size_t)index < values.size())
		value = values[index];
	return value;
}

int IniKey::SetValue(const std::string& newValue, const int index)
{
	if (index == -1)
	{
		values.push_back(newValue);
		return 1;
	}
	else if ((size_t)index < values.size())
	{
		if (values[index] == newValue)
			return 0;
	}
	else
	{
		values.resize(index + 1);
	}

	values[index] = newValue;
	return 1;
}

int IniKey::SetValues(const Array<std::string>& newValues)
{
	if (newValues == values)
		return 0;

	values = newValues;
	return 1;
}

int IniKey::SetIndexed(bool newIndexed)
{
	// 1 only for a change, as SetValue's
	int result = indexed != newIndexed ? 1 : 0;
	indexed = newIndexed;
	return result;
}