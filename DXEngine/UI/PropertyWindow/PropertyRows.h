#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class UObject;

// What the selected row's value column edits with, as the original's
// property window has each (bugs.md D.81): an edit box, a combo, a byte's
// edit and track bar, a colour's swatch and its "...", or nothing
enum class PropertyEditor
{
	None,
	Text,
	Combo,
	Slider,
	Color
};

// A row of a property window: a category, an object (a class's config page,
// EDITACTOR's actor), a property, an array's element or a struct's member.
// What is under a row is made as it opens and dropped as it closes, so each
// opening shows the values then.
class PropertyRow
{
public:
	virtual ~PropertyRow() = default;

	const std::string& GetCaption() const { return Caption; }
	PropertyRow* GetParent() const { return Parent; }
	int GetDepth() const { return Depth; }
	bool IsExpanded() const { return Expanded; }
	const std::vector<std::unique_ptr<PropertyRow>>& GetChildren() const { return Children; }

	// A box before its name, which opens it: a category's, an object's and a
	// struct's at a click, an array's at a double click
	virtual bool HasBox() { return false; }
	virtual bool OpensByDoubleClick() { return false; }
	void Expand();
	void Collapse();
	// What is under it made again, the rows open before open again: after an
	// array's Add, Empty, Insert or Delete, and a Reset To Defaults
	void Rebuild();

	// Whether it has a value column: a category's and an object's name runs
	// across
	virtual bool HasValue() { return true; }
	// The value column's text, and how the value is edited
	virtual std::string GetValue() { return {}; }
	virtual PropertyEditor GetEditor() { return PropertyEditor::None; }
	virtual std::vector<std::string> GetChoices() { return {}; }
	// The choice the value is, -1 for none
	virtual int GetChoice() { return -1; }
	// The text committed: an edit box's at Enter, a combo's pick, the track
	// bar's value; false where nothing took it
	virtual bool SetValue(const std::string& text) { return false; }
	// A colour's swatch, 0xAARRGGBB
	virtual bool GetColor(uint32_t& argb) { return false; }
	// An object reference (not a class's): its edit box and the original's
	// Clear, Use and "..." beside it (OBSERVED, EDITACTOR's window)
	virtual bool IsObjectReference() { return false; }

	// An object row, whose Reset To Defaults a config page shows while it or a
	// row under it is selected
	virtual bool IsObjectRow() { return false; }
	virtual bool HasResetToDefaults() { return false; }
	virtual void ResetToDefaults() {}

	// A dynamic array (its Add and Empty) and its elements (their Insert and
	// Delete)
	virtual bool IsDynamicArray() { return false; }
	virtual bool IsDynamicElement() { return false; }
	virtual void ArrayAdd() {}
	virtual void ArrayEmpty() {}
	virtual void ElementInsert() {}
	virtual void ElementDelete() {}

	// Itself or the nearest row above it that is an object row; null for none
	PropertyRow* GetObjectRow();

protected:
	virtual void MakeChildren(std::vector<std::unique_ptr<PropertyRow>>& rows) {}

	std::string Caption;

private:
	PropertyRow* Parent = nullptr;
	int Depth = 0;
	bool Expanded = false;
	std::vector<std::unique_ptr<PropertyRow>> Children;
};

// The Advanced Options' rows: the .int files' Preferences lines under
// "Advanced Options"
std::vector<std::unique_ptr<PropertyRow>> MakeAdvancedOptionsRows();

// EDITACTOR's rows: the object's editable properties by their categories
std::vector<std::unique_ptr<PropertyRow>> MakeObjectRows(UObject* obj);

// Every row, each opened in turn, to the log a line each -- its name, value,
// editor, a combo's choices, its buttons -- what the harness compares with
// the original's window walked (DXE_PROPERTY_DUMP)
void DumpPropertyRows(const std::vector<std::unique_ptr<PropertyRow>>& rows);
