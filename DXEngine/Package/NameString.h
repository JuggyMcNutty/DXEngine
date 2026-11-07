#pragma once

#include "Utils/ScriptString.h"

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>

class NameString
{
public:
	NameString() { }
	NameString(const char* str) { GetIndex(str); }
	NameString(const std::string& str) { GetIndex(str); }
	// A name from a script's string, 8-bit, each character past Latin-1 a '?'
	NameString(const ScriptString& str) { GetIndex(ScriptToLatin1(str)); }
	NameString(const NameString& other) = default;
	NameString& operator=(const NameString&) = default;

	bool IsNone() const { return CompareIndex == 0; }

	// The name's text: the spelling it was first made in, whatever case it
	// is asked in since, as the original's name table answers every spelling
	// of a name with its entry's (FName::FName, Core.dll 0x1014b950;
	// dx-reverse-info core-dll.md, names hashed and compared): DeusEx.u's
	// annadies for the conversations' AnnaDies, LOCATION as Location. A name
	// the original makes at its start (None, Class, System, User and the rest
	// of its hard-coded names) is spelt as it spells it from the start.
	const std::string& ToString() const { return Names[SpelledIndex]; }

	// A name's text as the table compares it, A to Z in upper case and every
	// other byte as it is (appStricmp's fold): a key that finds a name in any
	// case without making one, as a package's file does.
	static std::string Folded(const std::string& value);

	bool operator==(const char* other) const { return *this == NameString(other); }
	bool operator==(const std::string& other) const { return *this == NameString(other); }
	bool operator!=(const char* other) const { return *this != NameString(other); }
	bool operator!=(const std::string& other) const { return *this != NameString(other); }

	bool operator==(const NameString& other) const { return CompareIndex == other.CompareIndex; }
	bool operator!=(const NameString& other) const { return CompareIndex != other.CompareIndex; }
	bool operator<(const NameString& other) const { return CompareIndex < other.CompareIndex; }
	bool operator>(const NameString& other) const { return CompareIndex > other.CompareIndex; }
	bool operator<=(const NameString& other) const { return CompareIndex <= other.CompareIndex; }
	bool operator>=(const NameString& other) const { return CompareIndex >= other.CompareIndex; }

	int GetCompareIndex() const { return CompareIndex; }

	// A name made for an object the engine names itself (an actor spawned,
	// an AI event's object): the collector may delete it once nothing marks
	// it, as the original's deletes every name nothing reaches
	// (dx-reverse-info core-dll.md, Garbage collection). Every other name
	// is kept: a name looked up by its spelling, from a package, an ini, the
	// script or C++, is kept from then on.
	static NameString Collectable(const std::string& value);

	// The collection: BeginMark, then Mark for every name a live holder has,
	// then Sweep deletes every collectable name not marked, its slots taken
	// again by the next names made, last freed first; it hands back the
	// compare indexes it freed.
	static void BeginMark();
	void Mark() const
	{
		if (CompareIndex < (int)Marks.size())
			Marks[CompareIndex] |= MarkedFlag;
		if (SpelledIndex < (int)Marks.size())
			Marks[SpelledIndex] |= MarkedFlag;
		if (Recording && CompareIndex < (int)Marks.size() && SpelledIndex < (int)Marks.size() && IsCollectable())
			Recording->push_back({ CompareIndex, SpelledIndex });
	}
	// While set, Mark lists each collectable name it marks, as compare and
	// spelled index, for MarkIndexes to mark again (GC::Collect's walk of
	// the code, kept from a list until code is loaded again).
	static std::vector<std::pair<int, int>>* Recording;
	static void MarkIndexes(int compare, int spelled)
	{
		if (compare < (int)Marks.size())
			Marks[compare] |= MarkedFlag;
		if (spelled < (int)Marks.size())
			Marks[spelled] |= MarkedFlag;
	}
	static std::vector<int> Sweep();
	static size_t Count() { return Names.size() - Available.size(); }

	// Whether the collector may delete it: a cache keyed by name indexes
	// keeps none such, as its index may be taken by another name.
	bool IsCollectable() const { return (Marks[CompareIndex] | Marks[SpelledIndex]) & CollectableFlag; }

private:
	int CompareIndex = 0;
	int SpelledIndex = 0;

	enum : uint8_t { CollectableFlag = 1, MarkedFlag = 2 };

	static int NewSlot(const std::string& value)
	{
		if (!Available.empty())
		{
			int index = Available.back();
			Available.pop_back();
			Names[index] = value;
			Marks[index] = 0;
			return index;
		}
		Names.push_back(value);
		Marks.push_back(0);
		FirstSpelled.push_back(0);
		return (int)Names.size() - 1;
	}

	// The spelling a name new to the table is kept in: the original's for a
	// name it makes at its start, else the one it is made in.
	static const std::string& FirstSpelling(const std::string& folded, const std::string& value);

	void GetIndex(const std::string& value, bool collectable = false)
	{
		// Have we seen this spelling before?
		auto it = SpellStringToIndex.find(value);
		if (it != SpellStringToIndex.end())
		{
			CompareIndex = it->second.first;
			SpelledIndex = it->second.second;
			if (!collectable)
			{
				Marks[CompareIndex] &= ~CollectableFlag;
				Marks[SpelledIndex] &= ~CollectableFlag;
			}
			return;
		}

		// Initialize list with None as index 0
		if (Names.empty())
		{
			Names.push_back("None");
			Marks.push_back(0);
			FirstSpelled.push_back(0);
			CompareStringToIndex["NONE"] = 0;
			SpellStringToIndex["None"] = { 0, 0 };
		}

		// Any empty name string means None
		if (value.empty())
		{
			CompareIndex = 0;
			SpelledIndex = 0;
			return;
		}

		std::string compareValue = Folded(value);

		// A name made before in another case: its first spelling, the one
		// entry every spelling of it finds
		auto it2 = CompareStringToIndex.find(compareValue);
		if (it2 != CompareStringToIndex.end())
		{
			CompareIndex = it2->second;
			SpelledIndex = FirstSpelled[CompareIndex];
			if (!collectable)
			{
				Marks[CompareIndex] &= ~CollectableFlag;
				Marks[SpelledIndex] &= ~CollectableFlag;
			}
			// This spelling found at once from now on, but for a name the
			// collector may delete, which no spelling may outlive
			if (!IsCollectable())
				SpellStringToIndex[value] = { CompareIndex, SpelledIndex };
			return;
		}

		CompareIndex = NewSlot(compareValue);
		CompareStringToIndex[compareValue] = CompareIndex;

		const std::string& spelling = FirstSpelling(compareValue, value);
		SpelledIndex = NewSlot(spelling);
		FirstSpelled[CompareIndex] = SpelledIndex;
		SpellStringToIndex[spelling] = { CompareIndex, SpelledIndex };
		if (collectable)
		{
			Marks[CompareIndex] = CollectableFlag;
			Marks[SpelledIndex] = CollectableFlag;
			CollectableSpellings.push_back({ SpelledIndex, CompareIndex });
		}
		else if (spelling != value)
		{
			SpellStringToIndex[value] = { CompareIndex, SpelledIndex };
		}
	}

	static Array<std::string> Names;
	static std::vector<uint8_t> Marks;
	// Each compare entry's one spelling, by its compare index
	static std::vector<int> FirstSpelled;
	static std::vector<int> Available;
	static std::vector<std::pair<int, int>> CollectableSpellings; // spelled, compare
	static std::unordered_map<std::string, int> CompareStringToIndex;
	static std::unordered_map<std::string, std::pair<int, int>> SpellStringToIndex;
};
