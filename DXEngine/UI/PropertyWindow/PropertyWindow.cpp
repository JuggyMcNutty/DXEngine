#include "Precomp.h"
#include "PropertyWindow.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Utils/StrTools.h"
#include <dxwidgets/core/canvas.h>
#include <dxwidgets/core/font.h>
#include <dxwidgets/widgets/lineedit/lineedit.h>
#include <dxwidgets/widgets/dropdown/dropdown.h>
#include <dxwidgets/widgets/slider/slider.h>
#include <dxwidgets/widgets/pushbutton/pushbutton.h>
#include <dxwidgets/widgets/scrollbar/scrollbar.h>
#include <dxwidgets/widgets/colorpicker/colorpicker.h>
#include <dxwidgets/core/timer.h>

// The original's layout, in DIPs (OBSERVED.md): rows 16 high, a +/- box at
// 12 a depth, the value column from x 132 (its line at 134), an edit box to
// the column's end, a combo 21 high, a byte's edit 28 wide and its track
// bar, the object row's Reset To Defaults 118 wide, an array's Empty and Add,
// an element's Delete and Insert, a colour's "..." 14 wide
namespace Look
{
	constexpr double RowHeight = 16.0;
	constexpr double Indent = 12.0;
	constexpr double BoxLeft = 4.0;
	constexpr double BoxSize = 9.0;
	constexpr double NameLeft = 17.0;
	constexpr double ValueLeft = 132.0;
	constexpr double Divider = 134.0;
	constexpr double ComboHeight = 21.0;
	constexpr double EditHeight = 17.0;
	constexpr double SliderEditWidth = 28.0;
	constexpr double ResetWidth = 118.0;
	constexpr double EmptyWidth = 43.0;
	constexpr double AddWidth = 26.0;
	constexpr double DeleteWidth = 44.0;
	constexpr double InsertWidth = 39.0;
	constexpr double ColorButtonWidth = 14.0;
	// An object reference's Clear, Use and "..." (OBSERVED: 36, 28 and 14)
	constexpr double ClearWidth = 36.0;
	constexpr double UseWidth = 28.0;
	constexpr double BrowseWidth = 14.0;
	constexpr double ClientWidth = 344.0;
	constexpr double ClientHeight = 486.0;
	// The original's frame round its client area: 352x512 about 344x486
	constexpr double FrameWidth = 8.0;
	constexpr double FrameHeight = 26.0;
}

// A combo whose Up and Down move the tree's selection, as the original's
// (OBSERVED: a Down sent to a bool's combo selects the next row, the value
// unchanged); its other keys its own
class PropertyCombo : public Dropdown
{
public:
	PropertyCombo(Widget* parent) : Dropdown(parent) {}
	std::function<bool(InputKey)> FuncTreeKey;

protected:
	void OnKeyDown(InputKey key) override
	{
		if (FuncTreeKey && FuncTreeKey(key))
			return;
		Dropdown::OnKeyDown(key);
	}
};

class PropertyList : public Widget
{
public:
	PropertyList(Widget* parent, std::vector<std::unique_ptr<PropertyRow>> rows) : Widget(parent), Roots(std::move(rows))
	{
		SetStyleClass("treeview");
		Scroll = new Scrollbar(this);
		Scroll->FuncScroll = [this]() { PlaceEditors(); Update(); };
		Later = new Timer(this);
		Later->FuncExpired = [this]() {
			std::vector<std::function<void()>> pending;
			pending.swap(Pending);
			for (auto& fn : pending)
				fn();
		};
		RebuildVisible();
	}

protected:
	void OnPaint(Canvas* canvas) override
	{
		PseudoWidget body("treeview-body");
		Colorf text = body.GetStyleColor("color");
		Colorf selectionBackground = body.GetStyleColor("selection-background");
		Colorf selectionText = body.GetStyleColor("selection-color");
		Colorf grid = GetStyleColor("border-left-color");
		auto font = GetFont();
		auto vtp = canvas->verticalTextAlign(font);
		double width = ListWidth();
		double top = Scroll->GetPosition();
		for (size_t i = 0; i < Visible.size(); i++)
		{
			double y = i * Look::RowHeight - top;
			if (y + Look::RowHeight < 0.0 || y > GetHeight())
				continue;
			PropertyRow* row = Visible[i];
			double baseline = y + (Look::RowHeight - (vtp.bottom - vtp.top)) * 0.5 + vtp.baseline;
			double depth = row->GetDepth() * Look::Indent;
			bool selected = row == Selected;

			if (row->HasBox())
			{
				double bx = Look::BoxLeft + depth;
				double by = y + (Look::RowHeight - Look::BoxSize) * 0.5;
				canvas->fillRect(Rect::xywh(bx, by, Look::BoxSize, 1.0), text);
				canvas->fillRect(Rect::xywh(bx, by + Look::BoxSize - 1.0, Look::BoxSize, 1.0), text);
				canvas->fillRect(Rect::xywh(bx, by, 1.0, Look::BoxSize), text);
				canvas->fillRect(Rect::xywh(bx + Look::BoxSize - 1.0, by, 1.0, Look::BoxSize), text);
				canvas->fillRect(Rect::xywh(bx + 2.0, by + 4.0, Look::BoxSize - 4.0, 1.0), text);
				if (!row->IsExpanded())
					canvas->fillRect(Rect::xywh(bx + 4.0, by + 2.0, 1.0, Look::BoxSize - 4.0), text);
			}

			// The name, to the value column's line, a row of no value's
			// across; one longer ends in "..." ("MinDesiredFrame...")
			bool hasValue = row->HasValue();
			double nameLeft = Look::NameLeft + depth;
			double nameRight = hasValue ? Look::Divider : width;
			Rect nameBox = Rect::xywh(nameLeft, y, std::max(nameRight - nameLeft, 0.0), Look::RowHeight);
			if (selected)
				canvas->fillRect(nameBox, selectionBackground);
			canvas->drawTextEllipsis(font, Point(nameLeft + 1.0, baseline), nameBox, row->GetCaption(), selected ? selectionText : text);

			// The value column: the row's value, a colour's swatch
			if (hasValue)
			{
				canvas->fillRect(Rect::xywh(Look::Divider, y, 1.0, Look::RowHeight), grid);
				uint32_t argb = 0;
				if (row->GetEditor() == PropertyEditor::Color && row->GetColor(argb))
				{
					canvas->fillRect(Rect::xywh(Look::Divider + 4.0, y + 3.0, std::max(width - Look::Divider - 8.0, 0.0), Look::RowHeight - 6.0), Colorf::fromRgb(argb & 0xffffff));
				}
				else
				{
					Rect valueBox = Rect::xywh(Look::Divider + 4.0, y, std::max(width - Look::Divider - 6.0, 0.0), Look::RowHeight);
					canvas->drawTextEllipsis(font, Point(valueBox.x, baseline), valueBox, row->GetValue(), text);
				}
			}
			canvas->fillRect(Rect::xywh(0.0, y + Look::RowHeight - 1.0, width, 1.0), grid);
		}
	}

	bool OnMouseDown(const Point& pos, InputKey key) override
	{
		// The colour chooser open takes the window's input until it closes
		if (Picker)
			return true;
		SetFocus();
		if (key != InputKey::LeftMouse)
			return true;
		PropertyRow* row = RowAt(pos);
		if (!row)
			return true;
		// A click on a box opens or closes it and selects its row; an array's
		// box opens by a double click, its click selecting it (OBSERVED)
		if (row->HasBox() && !row->OpensByDoubleClick() && OnBox(row, pos))
			Toggle(row);
		Select(row);
		return true;
	}

	bool OnMouseDoubleclick(const Point& pos, InputKey key) override
	{
		if (Picker || key != InputKey::LeftMouse)
			return true;
		PropertyRow* row = RowAt(pos);
		if (row && row->HasBox())
		{
			Toggle(row);
			Select(row);
		}
		return true;
	}

	bool OnMouseWheel(const Point& pos, InputKey key) override
	{
		if (Picker)
			return true;
		double step = Look::RowHeight * 3.0;
		if (key == InputKey::MouseWheelUp)
			Scroll->SetPosition(std::max(Scroll->GetPosition() - step, 0.0));
		else if (key == InputKey::MouseWheelDown)
			Scroll->SetPosition(std::min(Scroll->GetPosition() + step, Scroll->GetMax()));
		PlaceEditors();
		Update();
		return true;
	}

	void OnKeyDown(InputKey key) override
	{
		if (!Picker)
			MoveSelection(key);
	}

	void OnGeometryChanged() override
	{
		double w = GetWidth(), h = GetHeight();
		double sw = Scroll->GetPreferredWidth();
		Scroll->SetFrameGeometry(Rect::xywh(w - sw, 0.0, sw, h));
		UpdateScroll();
		PlaceEditors();
	}

private:
	// Up and Down move the selection, its editor following, and Left and
	// Right as a list box's do, a row up and down (OBSERVED: Right on a
	// category moved the selection to the next row, opening nothing; no key
	// opens a row); false for a key that moves nothing
	bool MoveSelection(InputKey key)
	{
		int index = IndexOf(Selected);
		int count = (int)Visible.size();
		int page = std::max((int)(GetHeight() / Look::RowHeight) - 1, 1);
		int next = index;
		if (key == InputKey::Up || key == InputKey::Left) next = index - 1;
		else if (key == InputKey::Down || key == InputKey::Right) next = index < 0 ? 0 : index + 1;
		else if (key == InputKey::PageUp) next = index - page;
		else if (key == InputKey::PageDown) next = index < 0 ? 0 : index + page;
		else if (key == InputKey::Home) next = 0;
		else if (key == InputKey::End) next = count - 1;
		else
			return false;
		if (count == 0)
			return true;
		next = std::clamp(next, 0, count - 1);
		Select(Visible[next]);
		ScrollTo(next);
		return true;
	}

	// A key an editor hands the tree, moved after its event, the editor
	// going with the selection
	bool TreeKey(InputKey key)
	{
		if (Picker || (key != InputKey::Up && key != InputKey::Down && key != InputKey::PageUp && key != InputKey::PageDown))
			return false;
		Defer([this, key]() { MoveSelection(key); });
		return true;
	}

	// What must not run inside an editor's own event -- making the editors
	// again, which deletes it --, run after it
	void Defer(std::function<void()> fn)
	{
		Pending.push_back(std::move(fn));
		Later->Start(0, false);
	}

	double ListWidth()
	{
		return Scroll->IsVisible() ? GetWidth() - Scroll->GetPreferredWidth() : GetWidth();
	}

	void UpdateScroll()
	{
		double total = Visible.size() * Look::RowHeight;
		Scroll->SetRanges(GetHeight(), total);
		Scroll->SetVisible(total > GetHeight());
	}

	void AddVisible(PropertyRow* row)
	{
		Visible.push_back(row);
		if (row->IsExpanded())
		{
			for (const auto& child : row->GetChildren())
				AddVisible(child.get());
		}
	}

	void RebuildVisible()
	{
		Visible.clear();
		for (const auto& row : Roots)
			AddVisible(row.get());
		UpdateScroll();
		Update();
	}

	int IndexOf(PropertyRow* row)
	{
		auto it = std::find(Visible.begin(), Visible.end(), row);
		return it == Visible.end() ? -1 : (int)(it - Visible.begin());
	}

	PropertyRow* RowAt(const Point& pos)
	{
		if (pos.x >= ListWidth())
			return nullptr;
		int index = (int)std::floor((pos.y + Scroll->GetPosition()) / Look::RowHeight);
		return index >= 0 && index < (int)Visible.size() ? Visible[index] : nullptr;
	}

	bool OnBox(PropertyRow* row, const Point& pos)
	{
		double bx = Look::BoxLeft + row->GetDepth() * Look::Indent;
		return pos.x >= bx - 2.0 && pos.x < bx + Look::BoxSize + 2.0;
	}

	double RowTop(PropertyRow* row)
	{
		return IndexOf(row) * Look::RowHeight - Scroll->GetPosition();
	}

	void ScrollTo(int index)
	{
		double y = index * Look::RowHeight;
		if (y < Scroll->GetPosition())
			Scroll->SetPosition(y);
		else if (y + Look::RowHeight > Scroll->GetPosition() + GetHeight())
			Scroll->SetPosition(std::max(y + Look::RowHeight - GetHeight(), 0.0));
		PlaceEditors();
		Update();
	}

	void Toggle(PropertyRow* row)
	{
		CommitPending();
		if (row->IsExpanded())
			row->Collapse();
		else
			row->Expand();
		// A row under one closed goes with it: its parent selected
		if (Selected && !IsVisibleRow(Selected))
			Selected = row;
		RebuildVisible();
		ShowEditors();
	}

	bool IsVisibleRow(PropertyRow* row)
	{
		return IndexOf(row) >= 0;
	}

	void Select(PropertyRow* row)
	{
		if (row != Selected)
		{
			CommitPending();
			Selected = row;
			ShowEditors();
		}
		Update();
	}

	// An edit box's text typed and not committed: committed as by Enter as
	// its row loses the editor -- another row selected, a box clicked, a
	// button pressed (OBSERVED: another row selected commits it, the default,
	// the live object and the ini at once)
	void CommitPending()
	{
		if (!Selected)
			return;
		if (Edit && Edit->GetText() != EditShown)
			Commit(Edit->GetText());
		else if (TrackEdit && TrackEdit->GetText() != EditShown)
			Commit(TrackEdit->GetText());
	}

	// The row's names from the top, to find it again where its rows were
	// made again (a Reset To Defaults, an array's buttons)
	std::vector<std::string> PathOf(PropertyRow* row)
	{
		std::vector<std::string> path;
		for (PropertyRow* r = row; r; r = r->GetParent())
			path.insert(path.begin(), r->GetCaption());
		return path;
	}

	PropertyRow* FindPath(const std::vector<std::string>& path)
	{
		PropertyRow* found = nullptr;
		const std::vector<std::unique_ptr<PropertyRow>>* level = &Roots;
		for (const std::string& caption : path)
		{
			PropertyRow* next = nullptr;
			for (const auto& row : *level)
			{
				if (row->GetCaption() == caption)
				{
					next = row.get();
					break;
				}
			}
			if (!next)
				break;
			found = next;
			if (!next->IsExpanded())
				break;
			level = &next->GetChildren();
		}
		return found;
	}

	// A button's action, which may make the rows again: the selection found
	// again by its names -- none after an element's Insert or Delete
	// (OBSERVED) --, the editors and buttons made again after the button's
	// event, the button going with them
	void Act(const std::function<void()>& action, bool keepSelection = true)
	{
		CommitPending();
		std::vector<std::string> path = PathOf(Selected);
		action();
		RebuildVisible();
		Selected = keepSelection ? FindPath(path) : nullptr;
		Widget* editors[] = { Edit, Combo, Track, TrackEdit, ResetButton, AddButton, EmptyButton, InsertButton, DeleteButton, ColorButton, ClearButton, UseButton, BrowseButton };
		for (Widget* w : editors)
		{
			if (w)
				w->SetVisible(false);
		}
		Defer([this]() { ShowEditors(); Update(); });
		Update();
	}

	// A value committed: the row takes it, the editor shows the value as
	// the property's text again; no row is made again by it
	void Commit(const std::string& text)
	{
		if (!Selected)
			return;
		Selected->SetValue(text);
		EditShown = Selected->GetValue();
		if (Edit)
		{
			Edit->SetText(EditShown);
			Edit->SelectAll();
		}
		if (TrackEdit)
			TrackEdit->SetText(EditShown);
		if (Track)
		{
			Populating = true;
			Track->SetValue(std::atoi(Selected->GetValue().c_str()));
			Populating = false;
		}
		Update();
	}

	void HideEditors()
	{
		Widget* editors[] = { Edit, Combo, Track, TrackEdit, ResetButton, AddButton, EmptyButton, InsertButton, DeleteButton, ColorButton, ClearButton, UseButton, BrowseButton, Picker };
		for (Widget* w : editors)
			delete w;
		ClearButton = nullptr;
		UseButton = nullptr;
		BrowseButton = nullptr;
		Picker = nullptr;
		Edit = nullptr;
		Combo = nullptr;
		Track = nullptr;
		TrackEdit = nullptr;
		ResetButton = nullptr;
		AddButton = nullptr;
		EmptyButton = nullptr;
		InsertButton = nullptr;
		DeleteButton = nullptr;
		ColorButton = nullptr;
		ExtraChoice = -1;
	}

	PushButton* MakeButton(const std::string& text, const std::function<void()>& click)
	{
		auto button = new PushButton(this);
		button->SetText(text);
		button->OnClick = click;
		return button;
	}

	static std::string ButtonText(const char* key, const char* fallback)
	{
		std::string text;
		if (engine->packages->FindLocalized("Window", "General", key, text) && !text.empty())
			return text;
		return fallback;
	}

	LineEdit* MakeEdit(PropertyRow* row)
	{
		auto edit = new LineEdit(this);
		EditShown = row->GetValue();
		edit->SetText(EditShown);
		// Enter commits, the box showing the value as the property's text
		// again ("1500" typed shows "1500.000000"; OBSERVED)
		edit->FuncEnterPressed = [this, edit]() { Commit(edit->GetText()); };
		edit->FuncIgnoreKeyDown = [this](InputKey key) { return TreeKey(key); };
		return edit;
	}

	// The selected row's editor and buttons: the value's editor in its
	// column, the object row's Reset To Defaults, an array's Empty and Add,
	// an element's Delete and Insert (OBSERVED)
	void ShowEditors()
	{
		HideEditors();
		PropertyRow* row = Selected;
		if (!row)
			return;

		if (PropertyRow* object = row->GetObjectRow(); object && object->HasResetToDefaults())
			ResetButton = MakeButton(ButtonText("DefaultsButton", "Reset To Defaults"), [this, object]() { Act([object]() { object->ResetToDefaults(); }); });

		PropertyRow* array = row->IsDynamicArray() ? row : (row->IsDynamicElement() ? row->GetParent() : nullptr);
		if (array)
		{
			EmptyButton = MakeButton(ButtonText("EmptyButton", "Empty"), [this, array]() { Act([array]() { array->ArrayEmpty(); }); });
			AddButton = MakeButton(ButtonText("AddButton", "Add"), [this, array]() { Act([array]() { array->ArrayAdd(); }); });
		}
		if (row->IsDynamicElement())
		{
			DeleteButton = MakeButton(ButtonText("DeleteButton", "Delete"), [this, row]() { Act([row]() { row->ElementDelete(); }, false); });
			InsertButton = MakeButton(ButtonText("InsertButton", "Insert"), [this, row]() { Act([row]() { row->ElementInsert(); }, false); });
		}

		switch (row->GetEditor())
		{
		case PropertyEditor::Text:
			Edit = MakeEdit(row);
			// An object reference's Clear sets it None; Use, which takes
			// UnrealEd's selected object, and "...", its browser, take
			// nothing in the game (OBSERVED: the reference unchanged, no
			// window opened)
			if (row->IsObjectReference())
			{
				ClearButton = MakeButton(ButtonText("ClearButton", "Clear"), [this]() { Commit("None"); });
				UseButton = MakeButton(ButtonText("UseButton", "Use"), []() {});
				BrowseButton = MakeButton("...", []() {});
			}
			break;
		case PropertyEditor::Combo:
		{
			auto combo = new PropertyCombo(this);
			combo->FuncTreeKey = [this](InputKey key) { return TreeKey(key); };
			Combo = combo;
			Populating = true;
			std::vector<std::string> choices = row->GetChoices();
			for (const std::string& choice : choices)
				Combo->AddItem(choice);
			// A value no choice is (a console of no caption among the
			// classes, a class not listed) is an item of its own, selected,
			// so nothing changes until a pick (as ruled)
			int choice = row->GetChoice();
			if (choice < 0)
			{
				ExtraChoice = (int)choices.size();
				Combo->AddItem(row->GetValue());
				choice = ExtraChoice;
			}
			Combo->SetSelectedItem(choice);
			Populating = false;
			Combo->OnChanged = [this](int index) {
				if (Populating || !Selected || index < 0 || index == ExtraChoice)
					return;
				std::vector<std::string> choices = Selected->GetChoices();
				if (index < (int)choices.size())
					Commit(choices[index]);
			};
			break;
		}
		case PropertyEditor::Slider:
		{
			// A byte: its edit and a track bar of 0 to 255
			TrackEdit = MakeEdit(row);
			Track = new Slider(this);
			Track->SetMin(0);
			Track->SetMax(255);
			Populating = true;
			Track->SetValue(std::atoi(row->GetValue().c_str()));
			Populating = false;
			Track->ValueChanged = [this](int value) {
				if (!Populating)
					Commit(std::to_string(value));
			};
			break;
		}
		case PropertyEditor::Color:
			// "..." opens the colour chooser over the window, as the
			// original's opens its own; OK commits the colour as an edit
			// box's Enter does, its alpha as it was, Cancel or Escape leaves
			// it (as ruled)
			ColorButton = MakeButton(ButtonText("BrowseButton", "..."), [this]() { OpenPicker(); });
			break;
		default:
			break;
		}
		PlaceEditors();
		if (Edit)
			Edit->SetFocus();
		else if (TrackEdit)
			TrackEdit->SetFocus();
		else if (Combo)
			Combo->SetFocus();
		else
			SetFocus();
	}

	void OpenPicker()
	{
		uint32_t argb = 0;
		if (!Selected || Picker || !Selected->GetColor(argb))
			return;
		Picker = new ColorPicker(this);
		Picker->SetColor((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff);
		uint32_t alpha = argb >> 24;
		Picker->OnAccept = [this, alpha]() {
			if (!Picker)
				return;
			std::string text = "(R=" + std::to_string(Picker->GetRed()) + ",G=" + std::to_string(Picker->GetGreen()) +
				",B=" + std::to_string(Picker->GetBlue()) + ",A=" + std::to_string(alpha) + ")";
			Commit(text);
			ClosePicker();
		};
		Picker->OnCancel = [this]() { ClosePicker(); };
		PlaceEditors();
		Picker->SetFocus();
		Update();
	}

	// The chooser closed after its own event, the editors made again
	void ClosePicker()
	{
		if (Picker)
			Picker->SetVisible(false);
		Defer([this]() { ShowEditors(); Update(); });
	}

	void PlaceEditors()
	{
		PropertyRow* row = Selected;
		if (!row)
			return;
		double right = ListWidth();
		double y = RowTop(row);
		double valueRight = right;
		if (Picker)
		{
			// Under the colour's row where it fits, else over it
			double w = std::min(Picker->GetPreferredWidth(), std::max(right - 8.0, 100.0));
			double h = Picker->GetPreferredHeight();
			double py = y + Look::RowHeight + h <= GetHeight() ? y + Look::RowHeight : std::max(y - h, 0.0);
			Picker->SetFrameGeometry(Rect::xywh(std::max((right - w) * 0.5, 0.0), py, w, h));
		}

		if (ResetButton)
		{
			PropertyRow* object = row->GetObjectRow();
			ResetButton->SetFrameGeometry(Rect::xywh(right - Look::ResetWidth, RowTop(object), Look::ResetWidth, Look::RowHeight));
			ResetButton->SetVisible(IsVisibleRow(object));
		}
		if (AddButton && EmptyButton)
		{
			PropertyRow* array = row->IsDynamicArray() ? row : row->GetParent();
			double ay = RowTop(array);
			AddButton->SetFrameGeometry(Rect::xywh(right - Look::AddWidth, ay, Look::AddWidth, Look::RowHeight));
			EmptyButton->SetFrameGeometry(Rect::xywh(right - Look::AddWidth - Look::EmptyWidth, ay, Look::EmptyWidth, Look::RowHeight));
			if (array == row)
				valueRight = right - Look::AddWidth - Look::EmptyWidth;
		}
		if (InsertButton && DeleteButton)
		{
			InsertButton->SetFrameGeometry(Rect::xywh(right - Look::InsertWidth, y, Look::InsertWidth, Look::RowHeight));
			DeleteButton->SetFrameGeometry(Rect::xywh(right - Look::InsertWidth - Look::DeleteWidth, y, Look::DeleteWidth, Look::RowHeight));
			valueRight = right - Look::InsertWidth - Look::DeleteWidth;
		}
		if (ColorButton)
		{
			ColorButton->SetFrameGeometry(Rect::xywh(right - Look::ColorButtonWidth - 2.0, y, Look::ColorButtonWidth, Look::RowHeight));
			valueRight = right - Look::ColorButtonWidth - 2.0;
		}
		if (ClearButton && UseButton && BrowseButton)
		{
			BrowseButton->SetFrameGeometry(Rect::xywh(right - Look::BrowseWidth, y, Look::BrowseWidth, Look::RowHeight));
			UseButton->SetFrameGeometry(Rect::xywh(right - Look::BrowseWidth - Look::UseWidth, y, Look::UseWidth, Look::RowHeight));
			ClearButton->SetFrameGeometry(Rect::xywh(right - Look::BrowseWidth - Look::UseWidth - Look::ClearWidth, y, Look::ClearWidth, Look::RowHeight));
			valueRight = right - Look::BrowseWidth - Look::UseWidth - Look::ClearWidth;
		}
		if (Edit)
			Edit->SetFrameGeometry(Rect::xywh(Look::ValueLeft, y, std::max(valueRight - Look::ValueLeft, 10.0), Look::EditHeight));
		if (Combo)
			Combo->SetFrameGeometry(Rect::xywh(Look::ValueLeft - 1.0, y - 2.0, std::max(valueRight - Look::ValueLeft, 10.0), Look::ComboHeight));
		if (TrackEdit && Track)
		{
			TrackEdit->SetFrameGeometry(Rect::xywh(Look::ValueLeft, y, Look::SliderEditWidth, Look::EditHeight));
			Track->SetFrameGeometry(Rect::xywh(Look::ValueLeft + Look::SliderEditWidth - 1.0, y, std::max(valueRight - Look::ValueLeft - Look::SliderEditWidth + 1.0, 10.0), Look::EditHeight));
		}
	}

	std::vector<std::unique_ptr<PropertyRow>> Roots;
	std::vector<PropertyRow*> Visible;
	PropertyRow* Selected = nullptr;
	Scrollbar* Scroll = nullptr;
	LineEdit* Edit = nullptr;
	Dropdown* Combo = nullptr;
	Slider* Track = nullptr;
	LineEdit* TrackEdit = nullptr;
	PushButton* ResetButton = nullptr;
	PushButton* AddButton = nullptr;
	PushButton* EmptyButton = nullptr;
	PushButton* InsertButton = nullptr;
	PushButton* DeleteButton = nullptr;
	PushButton* ColorButton = nullptr;
	PushButton* ClearButton = nullptr;
	PushButton* UseButton = nullptr;
	PushButton* BrowseButton = nullptr;
	ColorPicker* Picker = nullptr;
	// The value's text the edit box showed as it was made or last
	// committed: a text unlike it is one typed
	std::string EditShown;
	// The combo's item that is the value no choice is, -1 for none
	int ExtraChoice = -1;
	bool Populating = false;
	Timer* Later = nullptr;
	std::vector<std::function<void()>> Pending;
};

/////////////////////////////////////////////////////////////////////////////

// The window's place: its [WindowPositions] line in the system ini,
// (X=,Y=,XL=,YL=) its frame's, as the original's window keeps its place
// (OBSERVED: Preferences=(X=0,Y=0,XL=352,YL=512)); with none, the screen's
// corner at the original's size
static Rect StoredPlace(const std::string& name)
{
	Rect place = Rect::xywh(0.0, 0.0, Look::ClientWidth, Look::ClientHeight);
	std::string text;
	if (name.empty() || !engine->packages->FindIniValue("system", "WindowPositions", name, text))
		return place;
	Array<UProperty::TextPair> pairs;
	UProperty::ParseStructText(text, pairs);
	for (const UProperty::TextPair& pair : pairs)
	{
		double value = std::atof(pair.Value.c_str());
		if (pair.Key == NameString("X")) place.x = value;
		else if (pair.Key == NameString("Y")) place.y = value;
		else if (pair.Key == NameString("XL")) place.width = std::max(value - Look::FrameWidth, 100.0);
		else if (pair.Key == NameString("YL")) place.height = std::max(value - Look::FrameHeight, 100.0);
	}
	return place;
}

PropertyWindow::PropertyWindow(Widget* owner, const std::string& title, const std::string& positionName, std::vector<std::unique_ptr<PropertyRow>> rows)
	: Widget(owner, WidgetType::Window), PositionName(positionName)
{
	SetWindowTitle(title);
	List = new PropertyList(this, std::move(rows));
	SetFrameGeometry(StoredPlace(PositionName));
}

PropertyWindow::~PropertyWindow()
{
	KeepPosition();
	if (OnDestroyed)
		OnDestroyed(this);
}

// Its place into the config, as the original's window keeps it as it goes,
// the file written with the next save of the config or at the exit
void PropertyWindow::KeepPosition()
{
	if (PositionName.empty() || !engine || !engine->packages)
		return;
	Rect frame = GetFrameGeometry();
	if (frame.width <= 0.0 || frame.height <= 0.0)
		return;
	std::string text = "(X=" + std::to_string((int)frame.x) + ",Y=" + std::to_string((int)frame.y) +
		",XL=" + std::to_string((int)(frame.width + Look::FrameWidth)) + ",YL=" + std::to_string((int)(frame.height + Look::FrameHeight)) + ")";
	engine->packages->SetIniValue("system", "WindowPositions", PositionName, text);
}

void PropertyWindow::Present()
{
	Show();
	ActivateWindow();
	List->SetFocus();
}

// The close box: the window goes, nothing written (OBSERVED)
void PropertyWindow::OnClose()
{
	delete this;
}

void PropertyWindow::OnGeometryChanged()
{
	List->SetFrameGeometry(Rect::xywh(0.0, 0.0, GetWidth(), GetHeight()));
}
