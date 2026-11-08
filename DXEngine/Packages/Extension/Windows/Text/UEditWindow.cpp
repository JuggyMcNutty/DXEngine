
#include "Precomp.h"
#include "Utils/UTF8Reader.h"
#include "UEditWindow.h"
#include "VM/ScriptCall.h"
#include "Packages/Core/Properties/UStrProperty.h"
#include "Engine.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Engine/Resources/USound.h"

namespace
{
	// The shared GC measuring with a window's own fonts and text settings, as
	// the original measures with the window's own GC; the GC's are put back
	// after.
	struct WindowTextGC
	{
		UGC* gc;
		UGC::TextSettings saved;
		WindowTextGC(UWindow* window) : gc(engine->dxgc), saved(gc ? gc->UseTextSettings(window) : UGC::TextSettings()) {}
		~WindowTextGC() { if (gc) gc->RestoreTextSettings(saved); }
	};

	bool IsSpace(char16_t c)
	{
		return (c >= 9 && c <= 13) || c == ' ';
	}
}

// The original's undo is a list of changes -- each a position, the text
// removed and the text put in -- capped at maxUndos, with typing straight
// after the last change joining it; the script's Ctrl+Z and Ctrl+Y call Undo
// and Redo (extension-dll.md, Small).
void UEditWindow::AddUndo(int pos, ScriptString removed, ScriptString inserted)
{
	undoList.resize(undoIndex);   // a new change drops what redo held

	if (!undoList.empty() && removed.empty() && !inserted.empty())
	{
		EditChange& last = undoList.back();
		if (last.removed.empty() && pos == last.pos + (int)last.inserted.size())
		{
			last.inserted += inserted;
			return;
		}
	}

	undoList.push_back({ pos, std::move(removed), std::move(inserted) });
	if (maxUndos() > 0 && (int)undoList.size() > maxUndos())
		undoList.erase(undoList.begin());
	undoIndex = (int)undoList.size();
}

// An undo or a redo: the change made again, the insertion point after what
// it put in, nothing selected.
void UEditWindow::ApplyChange(int pos, const ScriptString& from, const ScriptString& to)
{
	ScriptString text = Text();
	if (pos < 0 || pos + (int)from.size() > (int)text.size())
		return;
	text = text.substr(0, pos) + to + text.substr(pos + from.size());
	StoreText(text);
	SetTextChangedFlag(true);
	DispatchTextChanged(true);
	insertHookPos() = pos;
	SetCursorPoint(pos + (int)to.size(), false);
}

void UEditWindow::ClearUndo()
{
	undoList.clear();
	undoIndex = 0;
}

void UEditWindow::Redo()
{
	if (undoIndex >= (int)undoList.size())
		return;
	const EditChange& change = undoList[undoIndex];
	undoIndex++;
	ApplyChange(change.pos, change.removed, change.inserted);
}

void UEditWindow::Undo()
{
	if (undoIndex <= 0)
		return;
	undoIndex--;
	const EditChange& change = undoList[undoIndex];
	ApplyChange(change.pos, change.inserted, change.removed);
}

void UEditWindow::SetMaxUndos(int newMaxUndos)
{
	maxUndos() = newMaxUndos;
}

// The original's (XEditWindow::CopyToTextBuffer 0x1001e5f0 and
// PasteFromTextBuffer 0x1001e6b0): a copy or cut takes the selection,
// nothing with none; a paste inserts the buffer. The buffer is the desktop's
// clipboard, by choice (NATIVES.md, the UI): the 16-bit text goes out as
// UTF-8 and comes back as 16-bit characters, a carriage return left out.

void UEditWindow::Copy()
{
	int selStart = 0, selCount = 0;
	GetSelectedArea(selStart, selCount);
	if (selCount > 0)
		engine->window->SetClipboardText(ScriptToUTF8(Text().substr(selStart, selCount)));
}

void UEditWindow::Cut()
{
	int selStart = 0, selCount = 0;
	GetSelectedArea(selStart, selCount);
	if (selCount > 0)
	{
		engine->window->SetClipboardText(ScriptToUTF8(Text().substr(selStart, selCount)));
		DeleteChar(false, true);
	}
}

void UEditWindow::Paste()
{
	ScriptString text;
	for (char16_t c : UTF8ToScript(engine->window->GetClipboardText()))
	{
		if (c != '\r')
			text.push_back(c);
	}
	InsertText(text, true, {});
}

// The original's XEditWindow::DeleteChar (0x1001e560) and DeleteText
// (0x100201d0): without undo, the undo list cleared and the text marked
// changed; with nothing selected, the character before or after the
// insertion point selected; then the selection replaced by nothing.
void UEditWindow::DeleteChar(std::optional<bool> bBefore, std::optional<bool> bUndo)
{
	bool undo = bUndo.value_or(false);
	if (!undo)
	{
		ClearUndo();
		SetTextChangedFlag(true);
	}
	if (selectEnd() <= selectStart())
	{
		int pos = insertPos();
		if (bBefore.value_or(false))
		{
			if (pos > 0)
			{
				selectStart() = pos - 1;
				selectEnd() = pos;
			}
		}
		else if (pos < (int)Text().size())
		{
			selectStart() = pos;
			selectEnd() = pos + 1;
		}
	}
	ReplaceText({}, undo, false);
}

void UEditWindow::EnableEditing(std::optional<bool> bEdit)
{
	bEditable() = bEdit ? *bEdit : true;
}

// The original's XEditWindow::EnableSingleLineEditing (0x1001d620): a single
// line takes its text again, which drops its line breaks, and does not word
// wrap; several lines word wrap.
void UEditWindow::EnableSingleLineEditing(std::optional<bool> bSingle)
{
	bool single = bSingle.value_or(true);
	if (bSingleLine() == single)
		return;
	bSingleLine() = single;
	if (single)
	{
		ScriptString text = Text();
		SetText(text);
		SetWordWrap(false);
	}
	else
	{
		SetWordWrap(true);
	}
}

void UEditWindow::EnableUppercaseOnly(std::optional<bool> bUppercase)
{
	bUppercaseOnly() = bUppercase ? *bUppercase : true;
}

int UEditWindow::GetInsertionPoint()
{
	return insertPos();
}

// The original's (XEditWindow::GetSelectedArea 0x1001d5c0): no selection is
// -1 to -1, so 0 and 0.
void UEditWindow::GetSelectedArea(int& startPos, int& Count)
{
	startPos = std::max(selectStart(), 0);
	Count = std::max(selectEnd() - selectStart(), 0);
}

bool UEditWindow::HasTextChanged()
{
	return textChanged;
}

// The original's (XEditWindow::SetText 0x1001e260): the whole text selected
// and replaced as typing replaces it -- the undo list cleared, the change
// announced (TextChanged) -- then the insertion point at the start.
void UEditWindow::SetText(const ScriptString& NewText)
{
	selectStart() = 0;
	selectEnd() = (int)Text().size();
	insertPos() = selectEnd();
	InsertText(NewText, false, false);
	SetInsertionPoint(0, false);
}

// The original's (XEditWindow::AppendText 0x1001e310): put in at the end as
// typing puts it, then the insertion point at the start.
void UEditWindow::AppendText(const ScriptString& NewText)
{
	insertPos() = (int)Text().size();
	selectStart() = insertPos();
	selectEnd() = insertPos();
	InsertText(NewText, false, false);
	SetInsertionPoint(0, false);
}

// The text an edit leaves, laid out again (XEditWindow::ReplaceText
// 0x1001fe20); the insertion point stays where the edit put it.
void UEditWindow::StoreText(const ScriptString& text)
{
	if (Text() != text)
	{
		Text() = text;
		AskParentForReconfigure();
	}
}

// The original's XEditWindow::InsertText (0x1001e3c0): each character
// through the filter -- a single line drops line breaks, FilterChar may
// change or refuse one -- and nothing done when none passes; a change
// without undo clears the undo list. What passes replaces the selection
// (ReplaceText).
bool UEditWindow::InsertText(std::optional<ScriptString> InsertText, std::optional<bool> bUndo, std::optional<bool> bSelect)
{
	const ScriptString input = InsertText.value_or(ScriptString());
	ScriptString filtered;
	for (char16_t c : input)
	{
		if (bSingleLine() && c == '\n')
			continue;
		ScriptString ch(1, c);
		if (FilterChar(ch) && !ch.empty())
			filtered += ch[0];
	}
	if (!input.empty() && filtered.empty())
		return false;

	if (!bUndo.value_or(false))
	{
		ClearUndo();
		SetTextChangedFlag(true);
	}
	ReplaceText(filtered, bUndo.value_or(false), bSelect.value_or(false));
	return true;
}

// The original's XEditWindow::ReplaceText (0x1001fe20): the selection, or
// nothing at the insertion point, replaced by the text, cut to what MaxSize
// leaves room for -- a text already past MaxSize cut to it instead -- with
// an undo when asked. The change is announced up the parents (TextChanged)
// before the insertion point moves past what was put in, the hook at its
// start, so that with bSelect it is selected.
void UEditWindow::ReplaceText(ScriptString newText, bool bUndo, bool bSelect)
{
	ScriptString text = Text();
	int selCount = selectEnd() > selectStart() ? selectEnd() - selectStart() : 0;
	int insertLength = (int)newText.size();
	if (maxSize() > 0)
	{
		if ((int)text.size() > maxSize())
		{
			StoreText(text.substr(0, maxSize()));
			ClearUndo();
			SetTextChangedFlag(true);
			SetInsertionPoint(0, false);
			return;
		}
		if (selCount < insertLength)
		{
			int over = insertLength + (int)text.size() - maxSize() - selCount;
			if (over > 0)
			{
				insertLength -= over;
				if (insertLength < 0)
					return;
			}
		}
	}
	if (selCount <= 0 && insertLength <= 0)
		return;

	if (selectEnd() > selectStart())
		insertPos() = selectStart();
	int pos = std::clamp(insertPos(), 0, (int)text.size());
	selCount = std::min(selCount, (int)text.size() - pos);
	ScriptString inserted = newText.substr(0, insertLength);
	if (bUndo)
		AddUndo(pos, text.substr(pos, selCount), inserted);
	StoreText(text.substr(0, pos) + inserted + text.substr(pos + selCount));
	SetTextChangedFlag(true);
	DispatchTextChanged(HasTextChanged());
	insertHookPos() = pos;
	SetInsertionPoint(pos + insertLength, bSelect);
}

// The original's XEditWindow::FilterChar (0x10021830): upper case when only
// upper case is allowed, then the script's FilterChar, which may refuse the
// character or give another; with none, every character passes.
bool UEditWindow::FilterChar(ScriptString& ch)
{
	if (bUppercaseOnly() && ch[0] >= 'a' && ch[0] <= 'z')
		ch[0] -= 32;
	if (!FindEventFunction(this, "FilterChar"))
		return true;
	auto stringProp = GC::Alloc<UStrProperty>("", nullptr, ObjectFlags::NoFlags);
	return CallEvent(this, "FilterChar", { ExpressionValue::Variable(&ch, stringProp) }).ToBool();
}

bool UEditWindow::IsEditingEnabled()
{
	return bEditable();
}

bool UEditWindow::IsSingleLineEditingEnabled()
{
	return bSingleLine();
}

// The original's XEditWindow::MoveInsertionPoint (0x1001d250): up and down
// a row, and a page of rows -- as many as fit the height its parents leave in
// view, its clip rectangle's, at least one -- keeping the column the
// insertion point was last put in; left and right a character; a word either
// way within a row; a row's start and end; the text's start and end.
void UEditWindow::MoveInsertionPoint(uint8_t moveInsert, std::optional<bool> bDrag)
{
	bool drag = bDrag.value_or(false);
	const Array<TextRow>& r = Rows();
	int count = (int)r.size();
	int row = 0, col = 0, page = 1;
	if (count > 0)
	{
		PosToRowCol(insertPos(), row, col);
		page = std::max((int)(int64_t)(winClipRect().clipHeight / (lineHeight() + vSpace())), 1);
	}
	switch ((EMoveInsert)moveInsert)
	{
	case EMoveInsert::Up:
		if (count > 0)
			SetCursorRow(row - 1, drag);
		break;
	case EMoveInsert::Down:
		if (count > 0)
			SetCursorRow(row + 1, drag);
		break;
	case EMoveInsert::Left:
		SetCursorPoint(insertPos() - 1, drag);
		break;
	case EMoveInsert::Right:
		SetCursorPoint(insertPos() + 1, drag);
		break;
	case EMoveInsert::WordLeft:
		if (count > 0)
			SetCursorPoint(PrevWord(insertPos()), drag);
		break;
	case EMoveInsert::WordRight:
		if (count > 0)
			SetCursorPoint(NextWord(insertPos()), drag);
		break;
	case EMoveInsert::StartOfLine:
		if (count > 0)
			SetCursorPoint(r[row].startPos, drag);
		break;
	case EMoveInsert::EndOfLine:
		if (count > 0)
			SetCursorPoint(r[row].startPos + r[row].totalCount - 1, drag);
		break;
	case EMoveInsert::PageUp:
		if (count > 0)
			SetCursorRow(row - page, drag);
		break;
	case EMoveInsert::PageDown:
		if (count > 0)
			SetCursorRow(row + page, drag);
		break;
	case EMoveInsert::Home:
		SetCursorPoint(0, drag);
		break;
	case EMoveInsert::End:
		SetCursorPoint((int)Text().size(), drag);
		break;
	}
}

// The original's (XEditWindow::SetInsertionPoint 0x1001d4a0): the insertion
// point put there, dragging the selection or not.
void UEditWindow::SetInsertionPoint(int NewPos, std::optional<bool> bDrag)
{
	SetCursorPoint(NewPos, bDrag.value_or(false));
}

void UEditWindow::PlayEditSound(UObject* sound, std::optional<float> Volume, std::optional<float> Pitch)
{
	// What is the difference between this function and UWindow::PlaySound?
	PlaySound(sound, Volume, Pitch, {}, {});
}

void UEditWindow::SetEditCursor(std::optional<UObject*> newCursor, std::optional<UObject*> newCursorShadow, std::optional<Color> NewColor)
{
	if (newCursor.has_value())
		editCursor() = UObject::Cast<UTexture>(newCursor.value());
	if (newCursorShadow.has_value())
		editCursorShadow() = UObject::Cast<UTexture>(newCursorShadow.value());
	if (NewColor.has_value())
		editCursorColor() = NewColor.value();
}

void UEditWindow::SetEditSounds(std::optional<UObject*> newTypeSound, std::optional<UObject*> newDeleteSound, std::optional<UObject*> newEnterSound, std::optional<UObject*> newMoveSound)
{
	if (newTypeSound.has_value())
		typeSound() = UObject::Cast<USound>(newTypeSound.value());
	if (newDeleteSound.has_value())
		deleteSound() = UObject::Cast<USound>(newDeleteSound.value());
	if (newEnterSound.has_value())
		enterSound() = UObject::Cast<USound>(newEnterSound.value());
	if (newMoveSound.has_value())
		moveSound() = UObject::Cast<USound>(newMoveSound.value());
}

void UEditWindow::SetInsertionPointBlinkRate(std::optional<float> newBlinkStart, std::optional<float> newBlinkPeriod)
{
	if (newBlinkStart.has_value())
		blinkStart() = newBlinkStart.value();
	if (newBlinkPeriod.has_value())
		blinkPeriod() = newBlinkPeriod.value();
}

void UEditWindow::SetInsertionPointTexture(std::optional<UObject*> NewTexture, std::optional<Color> NewColor)
{
	if (NewTexture.has_value())
		insertTexture() = UObject::Cast<UTexture>(NewTexture.value());
	if (NewColor.has_value())
		insertColor() = NewColor.value();
}

void UEditWindow::SetInsertionPointType(uint8_t newType, std::optional<float> prefWidth, std::optional<float> prefHeight)
{
	insertType() = newType;
	if (prefWidth.has_value())
		insertPrefWidth() = prefWidth.value();
	if (prefHeight.has_value())
		insertPrefHeight() = prefHeight.value();
}

void UEditWindow::SetMaxSize(int newMaxSize)
{
	maxSize() = newMaxSize;
}

// The original's (XEditWindow::SetSelectedArea 0x1001d520): no word
// selection; the insertion point put at the start, then dragged to the end.
void UEditWindow::SetSelectedArea(int startPos, int Count)
{
	bSelectWords() = false;
	SetCursorPoint(startPos, false);
	SetCursorPoint(startPos + Count, true);
}

// The colour of the selected text (XEditWindow::SetSelectedAreaTextColor
// 0x1001dc30), the inverse of the selection's own.
void UEditWindow::SetSelectedAreaTextColor(std::optional<Color> NewColor)
{
	if (NewColor.has_value())
		inverseColor() = NewColor.value();
}

void UEditWindow::SetSelectedAreaTexture(std::optional<UObject*> NewTexture, std::optional<Color> NewColor)
{
	if (NewTexture.has_value())
		selectTexture() = UObject::Cast<UTexture>(NewTexture.value());
	if (NewColor.has_value())
		selectColor() = NewColor.value();
}

void UEditWindow::ClearTextChangedFlag()
{
	textChanged = false;
}

void UEditWindow::SetTextChangedFlag(std::optional<bool> bSet)
{
	textChanged = bSet ? *bSet : true;
}

// The original's (XEditWindow::ParentRequestedPreferredSize 0x10020ac0): a
// large text window's, a single line one row high when no height is given,
// and a width not given a space wider.
void UEditWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	ULargeTextWindow::ParentRequestedPreferredSize(bWidthSpecified, preferredWidth, bHeightSpecified, preferredHeight);

	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);
	if (bSingleLine() && !bHeightSpecified)
		preferredHeight = vMargin() + vMargin() + std::max(gc->GetFontHeight(false), 1.0f);
	if (!bWidthSpecified)
	{
		float spaceWidth = 0.0f, spaceHeight = 0.0f;
		gc->GetTextExtent(0.0f, spaceWidth, spaceHeight, " ");
		preferredWidth += spaceWidth;
	}
	gc->RestoreTextSettings(saved);
}

// The original's XEditWindow::Init (0x1001cea0): text from the top left, 3
// in from the sides, editable and on several lines, a white insertion point,
// grey selection with black text in it, up to 200000 characters and 64
// undos, a cursor shown and blinking each second after 0.75 s, its column
// and the last size unset, no special text, and selectable.
void UEditWindow::InitDefaults()
{
	ULargeTextWindow::InitDefaults();
	HAlign() = (uint8_t)EHAlign::Left;
	VAlign() = (uint8_t)EVAlign::Top;
	hMargin() = 3.0f;
	vMargin() = 0.0f;
	bEditable() = true;
	bSingleLine() = false;
	bUppercaseOnly() = false;
	insertPos() = 0;
	insertHookPos() = 0;
	insertType() = 0;
	selectStart() = 0;
	selectEnd() = 0;
	maxSize() = 200000;
	insertColor() = { 255, 255, 255, 255 };
	selectColor() = { 196, 196, 196, 255 };
	inverseColor() = { 0, 0, 0, 255 };
	maxUndos() = 64;
	blinkStart() = 0.75f;
	blinkPeriod() = 1.0f;
	dragDelay() = 0.0f;
	// The first blink's wait counts what of the frame has gone, as ShowCursor's
	blinkDelay() = GetTickOffset() + blinkStart();
	bCursorShowing() = true;
	bDragging() = false;
	bSelectWords() = false;
	insertX() = -1.0f;
	insertY() = 0.0f;
	insertWidth() = 0.0f;
	insertHeight() = 0.0f;
	insertPrefWidth() = 0.0f;
	insertPrefHeight() = 0.0f;
	showAreaX() = 0.0f;
	showAreaY() = 0.0f;
	showAreaWidth() = 0.0f;
	showAreaHeight() = 0.0f;
	insertPreferredCol() = -1.0f;
	lastConfigWidth() = -1.0f;
	lastConfigHeight() = -1.0f;
	selectStartRow() = -1;
	selectEndRow() = -1;
	selectStartX() = 0.0f;
	selectEndX() = 0.0f;
	bSpecialText() = false;
	bIsSelectable() = true;
	ComputeCursorConfig(true);
}

// The original's (XEditWindow::Tick 0x100207d0): editing on, the insertion
// point blinks on and off each half period; while a drag lasts, the mouse
// moves it again every 0.1 s, as far as the pointer has gone.
void UEditWindow::Tick(float timeElapsed)
{
	if (bEditable())
	{
		blinkDelay() -= timeElapsed;
		if (blinkDelay() < 0.0f)
		{
			float half = blinkPeriod() * 0.5f;
			blinkDelay() += half;
			if (blinkDelay() < 0.0f)
				blinkDelay() = half;
			bCursorShowing() = !bCursorShowing();
		}
	}
	if (bDragging())
	{
		dragDelay() -= timeElapsed;
		if (dragDelay() < 0.0f)
		{
			dragDelay() += 0.1f;
			if (dragDelay() < 0.0f)
				dragDelay() = 0.1f;
			float pointX = 0.0f, pointY = 0.0f;
			GetCursorPos(pointX, pointY);
			MouseMoved(pointX, pointY);
		}
	}
}

// The original's (XEditWindow::ConfigurationChanged 0x10020ba0): the rows
// made again for the new size and the insertion point placed again, keeping
// its column unless the size changed.
void UEditWindow::ConfigurationChanged()
{
	ULargeTextWindow::ConfigurationChanged();
	bool resized = lastConfigWidth() != Width() || lastConfigHeight() != Height();
	ComputeCursorConfig(resized);
	lastConfigWidth() = Width();
	lastConfigHeight() = Height();
}

// The original's (XEditWindow::VisibilityChanged 0x10020c60): shown, laid
// out again and its insertion point shown.
void UEditWindow::VisibilityChanged(bool bNewVisibility)
{
	ULargeTextWindow::VisibilityChanged(bNewVisibility);
	if (bNewVisibility)
	{
		AskParentForReconfigure();
		ShowCursor();
	}
}

// The original's XEditWindow::Draw (0x10020d00), editable and with the
// focus: the script's drawing, then within the margins each row by the
// alignments, a row at least 4 wide -- a selected stretch with the selection
// texture behind it, from half the vertical spacing above the row and a row
// and its spacing tall, its text in the inverse colour -- then the insertion
// point while it is blinked on. Otherwise as a large text window draws
// (XLargeTextWindow::Draw 0x1002e500): the rows within the margins. Either
// takes only the rows its clip rectangle reaches, from the one its top falls
// in to the one its bottom does.
void UEditWindow::DrawWindow(UGC* gc)
{
	UWindow::DrawWindow(gc);
	if (!gc->bDrawEnabled())
		return;

	const Array<TextRow>& r = Rows();
	int count = (int)r.size();
	float top = RowsTop(count);
	float step = lineHeight() + vSpace();
	Color color = gc->TextColor();
	bool editing = bEditable() && IsFocusWindow();
	int first = 0, last = count;
	if (step > 0.0f)
	{
		const ClipRect& clip = winClipRect();
		first = std::max((int)std::floor((clip.clipY + 0.00001f - top) / step), 0);
		last = std::min((int)std::ceil((clip.clipY + clip.clipHeight - top) / step), count);
	}

	gc->PushClip(gc->ScaleRect(Rectf::xywh(gc->offsetX + hMargin(), gc->offsetY + vMargin(), Width() - (hMargin() + hMargin()), Height() - (vMargin() + vMargin()))));
	if (!editing)
	{
		for (int i = first; i < last; i++)
			DrawRow(gc, r[i], RowLeft(r[i].xExtent), top + step * i, color, color, 0.0f, 0.0f);
		gc->PopClip();
		return;
	}

	float above = vSpace() - (float)(int64_t)(vSpace() * 0.5f);
	gc->SetTileColor(selectColor());
	for (int i = first; i < last; i++)
	{
		const TextRow& row = r[i];
		float rowWidth = std::max(row.xExtent, 4.0f);
		float x = RowLeft(rowWidth);
		float y = top + step * i;
		if (i == selectStartRow() || i == selectEndRow())
		{
			float from = i == selectStartRow() ? selectStartX() : x;
			float to = i == selectEndRow() ? selectEndX() : x + rowWidth;
			gc->DrawPattern(from, y - above, to - from, step, from, y - above, selectTexture());
			DrawRow(gc, row, x, y, color, inverseColor(), from, to);
		}
		else if (i < selectStartRow() || i > selectEndRow())
		{
			DrawRow(gc, row, x, y, color, color, 0.0f, 0.0f);
		}
		else
		{
			gc->DrawPattern(x, y - above, rowWidth, step, x, y - above, selectTexture());
			DrawRow(gc, row, x, y, inverseColor(), inverseColor(), 0.0f, 0.0f);
		}
	}
	gc->PopClip();

	if (bCursorShowing() && insertTexture())
	{
		gc->SetStyle(EDrawStyle::Masked);
		gc->SetTileColor(insertColor());
		gc->DrawPattern(insertX(), insertY(), insertWidth(), insertHeight(), 0.0f, 0.0f, insertTexture());
	}
}

// The original's XEditWindow::KeyPressed (0x10021290): the script's first;
// then, editing on, a typed character other than the console's ` and ~ is
// inserted with undo, and plays the type sound.
bool UEditWindow::KeyPressed(const ScriptString& key)
{
	bool handled = ULargeTextWindow::KeyPressed(key);
	if (bEditable() && key.size() == 1 && key[0] != '`' && key[0] != '~' && key[0] >= 0x20)
	{
		if (InsertText(key, true, false))
		{
			PlayEditSound(typeSound(), {}, {});
			return true;
		}
	}
	return handled;
}

// The original's XEditWindow::VirtualKeyPressed (0x10021480): Enter plays the
// enter sound, and in an editable single line activates the edit
// (EditActivated up the parents) without the script's hearing of it; all
// else is the script's, its Enter inserting a line break.
bool UEditWindow::VirtualKeyPressed(EInputKey key, bool bRepeat)
{
	if (key == IK_Enter)
	{
		PlayEditSound(enterSound(), {}, {});
		if (bSingleLine() && bEditable())
		{
			for (UWindow* cur = this; cur != nullptr; cur = cur->parentOwner())
			{
				if (cur->EditActivated(this, HasTextChanged()))
					break;
			}
			return true;
		}
	}
	return ULargeTextWindow::VirtualKeyPressed(key, bRepeat);
}

// The original's (XEditWindow::MouseButtonPressed 0x10021570): the script's
// first; then, editing on, the left button puts the insertion point where it
// is pressed -- with Shift held dragging the selection there -- and starts a
// drag, selecting whole words with Ctrl held or on a second click.
bool UEditWindow::MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks)
{
	SetFocusWindow(this);
	bool handled = ULargeTextWindow::MouseButtonPressed(pointX, pointY, button, numClicks);
	if (button == IK_LeftMouse && bEditable())
	{
		bSelectWords() = IsKeyDown(IK_Ctrl) || (numClicks - 1) % 2 == 1;
		bDragging() = true;
		dragDelay() = GetTickOffset() + 0.1f;
		SetInsertionPoint(XYToPos(pointX, pointY), IsKeyDown(IK_Shift));
		return true;
	}
	return handled;
}

// The original's (XEditWindow::MouseMoved 0x100216b0): the script's first;
// then, while a drag lasts, the insertion point dragged to the pointer.
void UEditWindow::MouseMoved(float newX, float newY)
{
	ULargeTextWindow::MouseMoved(newX, newY);
	if (bDragging())
		SetInsertionPoint(XYToPos(newX, newY), true);
}

// The original's (XEditWindow::MouseButtonReleased 0x10021760): the script's
// first; the left button's release ends the drag and the word selection.
bool UEditWindow::MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks)
{
	bool handled = ULargeTextWindow::MouseButtonReleased(pointX, pointY, button, numClicks);
	if (button == IK_LeftMouse)
	{
		bDragging() = false;
		bSelectWords() = false;
		if (bEditable())
			return true;
	}
	return handled;
}

void UEditWindow::DispatchTextChanged(bool modified)
{
	for (UWindow* cur = this; cur != nullptr; cur = cur->parentOwner())
	{
		if (cur->TextChanged(this, modified))
			break;
	}
}

// The rows of the text, wrapped at the width less the margins when word
// wrap is on (XLargeTextWindow::ConfigurationChanged 0x1002e430,
// GenerateLines 0x1002da70), and the line height, the font's at least 1.
const Array<UEditWindow::TextRow>& UEditWindow::Rows()
{
	float wrap = bWordWrap() ? Width() - (hMargin() + hMargin()) : 0.0f;
	if (rowsMade && rowsText == Text() && rowsWrap == wrap && rowsNormalFont == normalFont() && rowsBoldFont == boldFont() && rowsSpecialText == bSpecialText())
		return rows;

	WindowTextGC g(this);
	if (!g.gc)
		return rows;

	rows.clear();
	// The rows are laid out over a copy of the text, which the rows' places
	// index
	rowsText = Text();
	const char16_t* start = rowsText.data();
	const char16_t* end = start + rowsText.size();
	const char16_t* text = start;
	UGC::TextState state;
	while (true)
	{
		const char16_t* next = nullptr;
		UGC::TextState nextState;
		int length = 0;
		float width = 0.0f;
		if (!g.gc->ParseLine(text, state, end, wrap, next, nextState, length, width))
			break;
		TextRow row;
		row.xExtent = width;
		row.startPos = (int)(text - start);
		row.count = length;
		row.totalCount = (int)(next - text);
		row.state = state;
		rows.push_back(row);
		text = next;
		state = nextState;
	}
	if (!rows.empty())
		rows.back().totalCount++;
	lineHeight() = std::max(g.gc->GetFontHeight(false), 1.0f);

	rowsMade = true;
	rowsWrap = wrap;
	rowsNormalFont = normalFont();
	rowsBoldFont = boldFont();
	rowsSpecialText = bSpecialText();
	return rows;
}

// Where the block of that many rows starts down (XEditWindow::RowToY
// 0x1001e930): at the top margin, the bottom one, or centred by whole pixels.
float UEditWindow::RowsTop(int count)
{
	float textHeight = count * lineHeight();
	if (count > 1)
		textHeight += (count - 1) * vSpace();
	switch ((EVAlign)VAlign())
	{
	case EVAlign::Top:
		return vMargin();
	case EVAlign::Bottom:
		return Height() - textHeight - vMargin();
	default:
		return (float)(int64_t)((Height() - textHeight) * 0.5f);
	}
}

// Where a row of that width starts across (XEditWindow::ColToX 0x1001e9d0):
// at the left margin, the right one, or centred by whole pixels.
float UEditWindow::RowLeft(float rowWidth)
{
	switch ((EHAlign)HAlign())
	{
	case EHAlign::Left:
		return hMargin();
	case EHAlign::Right:
		return Width() - rowWidth - hMargin();
	default:
		return (float)(int64_t)((Width() - rowWidth) * 0.5f);
	}
}

// How far into a row a column lies: the widths of the characters before it
// (XGC::CharToPixel 0x10027fc0).
float UEditWindow::CharToPixel(UGC* gc, const TextRow& row, int col)
{
	const char16_t* start = rowsText.data() + row.startPos;
	const char16_t* end = start + row.count;
	const char16_t* p = start;
	UGC::TextState state = row.state;
	int index = 0;
	float x = 0.0f;
	while (true)
	{
		char16_t ch = gc->GetNextChar(p, state, end);
		if (ch == 0 || ch == '\n' || index >= col)
			break;
		index = (int)(p - start);
		if (UFont* font = state.bold ? gc->boldFont() : gc->normalFont())
			x += (float)font->GetPageGlyph(ch).USize;
	}
	return x;
}

// The column nearest a place in a row: before the first character whose
// middle lies past it (XGC::PixelToChar 0x10028050).
int UEditWindow::PixelToChar(UGC* gc, const TextRow& row, float x)
{
	const char16_t* start = rowsText.data() + row.startPos;
	const char16_t* end = start + row.count;
	const char16_t* p = start;
	UGC::TextState state = row.state;
	int index = 0;
	float pos = 0.0f;
	do
	{
		char16_t ch = gc->GetNextChar(p, state, end);
		if (ch == 0 || ch == '\n')
			break;
		UFont* font = state.bold ? gc->boldFont() : gc->normalFont();
		float width = font ? (float)font->GetPageGlyph(ch).USize : 0.0f;
		if (width * 0.5f + pos > x)
			break;
		pos += width;
		index = (int)(p - start);
	} while (x > pos);
	return index;
}

// The row at a place down, held to the rows (XEditWindow::YToRow 0x1001e740).
int UEditWindow::YToRow(float pointY)
{
	int count = (int)Rows().size();
	int row = (int)(int64_t)((pointY - RowsTop(count)) / (vSpace() + lineHeight()));
	return std::max(std::min(row, count - 1), 0);
}

// The column at a place across a row (XEditWindow::XToCol 0x1001e7f0).
int UEditWindow::XToCol(int row, float pointX)
{
	const Array<TextRow>& r = Rows();
	if (row < 0 || row >= (int)r.size())
		return 0;
	WindowTextGC g(this);
	if (!g.gc)
		return 0;
	return PixelToChar(g.gc, r[row], pointX - RowLeft(r[row].xExtent));
}

// The top of a row, the block at least a row tall (XEditWindow::RowToY
// 0x1001e930).
float UEditWindow::RowToY(int row)
{
	return RowsTop(std::max((int)Rows().size(), 1)) + (vSpace() + lineHeight()) * row;
}

// The place across of a column in a row (XEditWindow::ColToX 0x1001e9d0).
float UEditWindow::ColToX(int row, int col)
{
	const Array<TextRow>& r = Rows();
	bool valid = row >= 0 && row < (int)r.size();
	float left = RowLeft(valid ? r[row].xExtent : 0.0f);
	if (!valid)
		return left;
	WindowTextGC g(this);
	return left + (g.gc ? CharToPixel(g.gc, r[row], col) : 0.0f);
}

// The place in the text of a row's column, each held to what there is
// (XEditWindow::RowColToPos 0x1001eb30).
int UEditWindow::RowColToPos(int row, int col)
{
	const Array<TextRow>& r = Rows();
	int count = (int)r.size();
	row = std::max(std::min(row, count - 1), 0);
	int pos = 0;
	if (row < count)
		pos = std::max(std::min(col, r[row].totalCount - 1), 0) + r[row].startPos;
	return std::max(std::min(pos, (int)Text().size()), 0);
}

// The row and column of a place in the text: the last row starting at or
// before it (XEditWindow::PosToRowCol 0x1001eba0).
void UEditWindow::PosToRowCol(int pos, int& row, int& col)
{
	const Array<TextRow>& r = Rows();
	pos = std::max(std::min(pos, (int)Text().size()), 0);
	row = 0;
	col = 0;
	for (int i = (int)r.size() - 1; i >= 0; i--)
	{
		if (r[i].startPos <= pos)
		{
			row = i;
			col = pos - r[i].startPos;
			return;
		}
	}
}

// The place in the text at a point in the window (XEditWindow::XYToPos
// 0x1001ec10).
int UEditWindow::XYToPos(float pointX, float pointY)
{
	const Array<TextRow>& r = Rows();
	if (r.empty())
		return 0;
	int row = YToRow(pointY);
	return XToCol(row, pointX) + r[row].startPos;
}

// The point in the window of a place in the text (XEditWindow::PosToXY
// 0x1001ecd0).
void UEditWindow::PosToXY(int pos, float& pointX, float& pointY)
{
	int row = 0, col = 0;
	PosToRowCol(pos, row, col);
	pointY = RowToY(row);
	pointX = ColToX(row, col);
}

// The original's XEditWindow::ComputeCursorConfig (0x1001f680): the
// insertion point's size by its type -- Insert 2 wide (or its preferred
// width) and a row tall, centred on the place between two characters;
// Underscore and Block the character's width under or over it, Underscore 2
// tall, Block a row and half the spacing; the raw types the texture's size
// -- and its place, which sets its column when asked; the area to keep in
// view, the character and the insertion point; the selection's rows and
// places.
void UEditWindow::ComputeCursorConfig(bool bSetPrefCol)
{
	Rows();
	float charWidth = 8.0f;
	float charHeight = lineHeight();
	char16_t ch = insertPos() >= 0 && insertPos() < (int)Text().size() ? Text()[insertPos()] : 0;
	if (ch == 0 || IsSpace(ch))
		ch = ' ';
	{
		WindowTextGC g(this);
		if (g.gc)
			g.gc->GetTextExtent(0.0f, charWidth, charHeight, ScriptString(1, ch));
	}

	auto type = (EInsertionPointType)insertType();
	bool overChar = type == EInsertionPointType::Underscore || type == EInsertionPointType::Block || type == EInsertionPointType::RawOverlay;
	switch (type)
	{
	case EInsertionPointType::Insert:
		insertWidth() = insertPrefWidth() < 1.0f ? 2.0f : insertPrefWidth();
		insertHeight() = lineHeight();
		break;
	case EInsertionPointType::Underscore:
		insertWidth() = std::max(charWidth, 1.0f);
		insertHeight() = insertPrefHeight() < 1.0f ? 2.0f : insertPrefHeight();
		break;
	case EInsertionPointType::Block:
		insertWidth() = std::max(charWidth, 1.0f);
		insertHeight() = vSpace() - (float)(int64_t)(vSpace() * 0.5f) + lineHeight();
		break;
	default:
		insertWidth() = insertPrefWidth() < 1.0f ? (insertTexture() ? (float)insertTexture()->USize() : 0.0f) : insertPrefWidth();
		insertHeight() = insertPrefHeight() < 1.0f ? (insertTexture() ? (float)insertTexture()->VSize() : 0.0f) : insertPrefHeight();
		break;
	}

	PosToXY(insertPos(), insertX(), insertY());
	if (bSetPrefCol)
		insertPreferredCol() = insertX();
	showAreaX() = insertX();
	showAreaY() = insertY();
	showAreaWidth() = charWidth;
	showAreaHeight() = charHeight;
	if (overChar)
	{
		insertX() += (float)(int64_t)((charWidth - insertWidth()) * 0.5f);
		insertY() += (float)(int64_t)(lineHeight() - insertHeight());
	}
	else
	{
		insertX() -= (float)(int64_t)(insertWidth() * 0.5f);
		insertY() += (float)(int64_t)((lineHeight() - insertHeight()) * 0.5f);
	}
	if (showAreaX() > insertX())
	{
		showAreaWidth() = showAreaX() - insertX() + charWidth;
		showAreaX() = insertX();
	}
	if (showAreaY() > insertY())
	{
		showAreaHeight() = showAreaY() - insertY() + charHeight;
		showAreaY() = insertY();
	}
	if (showAreaX() + showAreaWidth() < insertX() + insertWidth())
		showAreaWidth() = insertX() + insertWidth() - showAreaX();
	if (showAreaY() + showAreaHeight() < insertY() + insertHeight())
		showAreaHeight() = insertY() + insertHeight() - showAreaY();

	selectStartX() = 0.0f;
	selectEndX() = 0.0f;
	selectStartRow() = -1;
	selectEndRow() = -1;
	if (selectStart() < selectEnd())
	{
		int row = 0, col = 0;
		PosToRowCol(selectStart(), row, col);
		selectStartRow() = row;
		selectStartX() = ColToX(row, col);
		PosToRowCol(selectEnd(), row, col);
		selectEndRow() = row;
		selectEndX() = ColToX(row, col);
	}
}

// The original's (XEditWindow::ShowCursor 0x1001ef40): the insertion point
// blinked on, its blink starting over, and its area, a pixel wider each
// side, asked to be in view.
void UEditWindow::ShowCursor()
{
	bCursorShowing() = true;
	blinkDelay() = GetTickOffset() + blinkStart();
	AskParentToShowArea(showAreaX() - 1.0f, showAreaY(), showAreaWidth() + 2.0f, showAreaHeight());
}

// The original's (XEditWindow::ComputeSelectArea 0x1001f010): from the hook
// to the insertion point, either way, whole words when selecting words --
// the insertion point then at the selection's far end -- else none.
void UEditWindow::ComputeSelectArea()
{
	if (insertPos() != insertHookPos() || bSelectWords())
	{
		bool forward = insertHookPos() <= insertPos();
		selectStart() = std::min(insertHookPos(), insertPos());
		selectEnd() = std::max(insertHookPos(), insertPos());
		if (bSelectWords())
		{
			selectStart() = StartOfWord(selectStart());
			selectEnd() = EndOfWord(selectEnd());
			insertPos() = forward ? selectEnd() : selectStart();
		}
	}
	else
	{
		selectStart() = -1;
		selectEnd() = -1;
	}
}

// The original's (XEditWindow::SetCursorPoint 0x1001f140): the insertion
// point at a place in the text, the hook with it unless dragging, its
// column set from it.
void UEditWindow::SetCursorPoint(int pos, bool bDrag)
{
	pos = std::max(std::min(pos, (int)Text().size()), 0);
	insertPos() = pos;
	if (!bDrag)
		insertHookPos() = pos;
	ComputeSelectArea();
	ComputeCursorConfig(true);
	ShowCursor();
}

// The original's (XEditWindow::SetCursorRow 0x1001f200): the insertion
// point on a row, held to the rows, at the column it was put in.
void UEditWindow::SetCursorRow(int row, bool bDrag)
{
	const Array<TextRow>& r = Rows();
	int count = (int)r.size();
	row = std::max(std::min(row, count - 1), 0);
	int pos = row < count ? XToCol(row, insertPreferredCol()) + r[row].startPos : 0;
	pos = std::max(std::min(pos, (int)Text().size()), 0);
	insertPos() = pos;
	if (!bDrag)
		insertHookPos() = pos;
	ComputeSelectArea();
	ComputeCursorConfig(false);
	ShowCursor();
}

// Back to the whitespace before a word (XEditWindow::StartOfWord 0x1001f300).
int UEditWindow::StartOfWord(int pos)
{
	const ScriptString& text = Text();
	int i = std::min(pos, (int)text.size()) - 1;
	while (i >= 0 && !IsSpace(text[i]))
		i--;
	return i + 1;
}

// On to the whitespace after a word (XEditWindow::EndOfWord 0x1001f350).
int UEditWindow::EndOfWord(int pos)
{
	const ScriptString& text = Text();
	int i = std::max(pos, 0);
	while (i < (int)text.size() && !IsSpace(text[i]))
		i++;
	return i;
}

// The original's (XEditWindow::NextWord 0x1001f3b0): within the row, past
// the word and the spaces after it to the next word's start; from the row's
// last character, the next row's start; with no word left in the row, the
// row's end.
int UEditWindow::NextWord(int pos)
{
	const Array<TextRow>& r = Rows();
	int row = 0, col = 0;
	PosToRowCol(pos, row, col);
	if (row < 0 || row >= (int)r.size())
		return 0;
	const TextRow& line = r[row];
	const ScriptString& text = Text();
	int end = line.startPos + line.count;
	if (pos >= end)
		return line.startPos + line.totalCount;
	bool sawSpace = false;
	int i = pos;
	while (true)
	{
		if (i >= end)
			return line.startPos + line.totalCount - 1;
		if (IsSpace(text[i]))
			sawSpace = true;
		else if (sawSpace)
			return i;
		i++;
	}
}

// The original's (XEditWindow::PrevWord 0x1001f550): within the row, back
// past the spaces and the word before to that word's start; from the row's
// start, the place before it.
int UEditWindow::PrevWord(int pos)
{
	const Array<TextRow>& r = Rows();
	int row = 0, col = 0;
	PosToRowCol(pos, row, col);
	if (row < 0 || row >= (int)r.size())
		return 0;
	int rowStart = r[row].startPos;
	int result = pos - 1;
	if (pos != rowStart)
	{
		const ScriptString& text = Text();
		bool sawWord = false;
		int i = pos;
		while (--i >= rowStart)
		{
			if (!IsSpace(text[i]))
				sawWord = true;
			else if (sawWord)
				break;
		}
		result = i + 1;
	}
	return std::max(result, 0);
}

// A row's characters from a place across, as the original's
// XGC::DrawTextLine and DrawTextChar draw them: each in the selected colour
// while the pen is within [selectedFrom, selectedTo).
void UEditWindow::DrawRow(UGC* gc, const TextRow& row, float x, float y, const Color& color, const Color& selectedColor, float selectedFrom, float selectedTo)
{
	const char16_t* start = rowsText.data() + row.startPos;
	const char16_t* end = start + row.count;
	const char16_t* p = start;
	UGC::TextState state = row.state;
	while (true)
	{
		bool selected = x >= selectedFrom && x < selectedTo;
		char16_t ch = gc->GetNextChar(p, state, end);
		if (ch == 0 || ch == '\n')
			break;
		UFont* font = state.bold ? gc->boldFont() : gc->normalFont();
		if (!font)
			continue;
		float height = 0.0f;
		gc->DrawChar(font, ch, state.ownColor ? state.color : (selected ? selectedColor : color), state.accel, x, y, x, height);
	}
}
