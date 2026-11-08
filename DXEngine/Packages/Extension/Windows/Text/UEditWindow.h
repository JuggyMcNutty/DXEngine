#pragma once

#include "ULargeTextWindow.h"
#include "Packages/Extension/Windows/UGC.h"

class UEditWindow : public ULargeTextWindow
{
public:
	using ULargeTextWindow::ULargeTextWindow;

	void ClearTextChangedFlag();
	void ClearUndo();
	void Copy();
	void Cut();
	void DeleteChar(std::optional<bool> bBefore, std::optional<bool> bUndo);
	void EnableEditing(std::optional<bool> bEdit);
	void EnableSingleLineEditing(std::optional<bool> bSingle);
	void EnableUppercaseOnly(std::optional<bool> bUppercase);
	int GetInsertionPoint();
	void GetSelectedArea(int& startPos, int& Count);
	bool HasTextChanged();
	bool InsertText(std::optional<ScriptString> InsertText, std::optional<bool> bUndo, std::optional<bool> bSelect);
	bool IsEditingEnabled();
	bool IsSingleLineEditingEnabled();
	void MoveInsertionPoint(uint8_t moveInsert, std::optional<bool> bDrag);
	void Paste();
	void PlayEditSound(UObject* PlaySound, std::optional<float> Volume, std::optional<float> Pitch);
	void Redo();
	void SetEditCursor(std::optional<UObject*> newCursor, std::optional<UObject*> newCursorShadow, std::optional<Color> NewColor);
	void SetEditSounds(std::optional<UObject*> typeSound, std::optional<UObject*> deleteSound, std::optional<UObject*> enterSound, std::optional<UObject*> moveSound);
	void SetInsertionPoint(int NewPos, std::optional<bool> bDrag);
	void SetInsertionPointBlinkRate(std::optional<float> blinkStart, std::optional<float> blinkPeriod);
	void SetInsertionPointTexture(std::optional<UObject*> NewTexture, std::optional<Color> NewColor);
	void SetInsertionPointType(uint8_t newType, std::optional<float> prefWidth, std::optional<float> prefHeight);
	void SetMaxSize(int newMaxSize);
	void SetMaxUndos(int newMaxUndos);
	void SetSelectedArea(int startPos, int Count);
	void SetSelectedAreaTextColor(std::optional<Color> NewColor);
	void SetSelectedAreaTexture(std::optional<UObject*> NewTexture, std::optional<Color> NewColor);
	void SetTextChangedFlag(std::optional<bool> bSet);
	void Undo();

	void InitDefaults() override;
	void ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight) override;
	bool FilterChar(ScriptString& ch);
	void Tick(float timeElapsed) override;
	void ConfigurationChanged() override;
	void VisibilityChanged(bool bNewVisibility) override;
	void DrawWindow(UGC* gc) override;
	bool KeyPressed(const ScriptString& key) override;
	bool VirtualKeyPressed(EInputKey key, bool bRepeat) override;
	bool MouseButtonPressed(float pointX, float pointY, EInputKey button, int numClicks) override;
	void MouseMoved(float newX, float newY) override;
	bool MouseButtonReleased(float pointX, float pointY, EInputKey button, int numClicks) override;
	void SetText(const ScriptString& NewText) override;
	void AppendText(const ScriptString& NewText) override;
	void StoreText(const ScriptString& text);

	// One row of the text as the GC breaks it (the original's XTextRowData):
	// its width, where it starts, its characters without and with the spaces
	// it ends with -- the last row's one more, for the place after the text --
	// and the text state it starts in.
	struct TextRow
	{
		float xExtent = 0.0f;
		int startPos = 0;
		int count = 0;
		int totalCount = 0;
		UGC::TextState state;
	};

	// The undo list: each change a position, the text removed and the text
	// put in (extension-dll.md, Small). Changes [0, currentUndo) are applied;
	// what follows is what redo replays.
	struct EditChange
	{
		int pos = 0;
		ScriptString removed;
		ScriptString inserted;
	};
	Array<EditChange> undoList;

	void AddUndo(int pos, ScriptString removed, ScriptString inserted);
	void ClampUndo();

	BitfieldBool bCursorShowing() { return BoolValue(PropOffsets_EditWindow.bCursorShowing); }
	BitfieldBool bDragging() { return BoolValue(PropOffsets_EditWindow.bDragging); }
	BitfieldBool bEditable() { return BoolValue(PropOffsets_EditWindow.bEditable); }
	BitfieldBool bSelectWords() { return BoolValue(PropOffsets_EditWindow.bSelectWords); }
	BitfieldBool bSingleLine() { return BoolValue(PropOffsets_EditWindow.bSingleLine); }
	BitfieldBool bUppercaseOnly() { return BoolValue(PropOffsets_EditWindow.bUppercaseOnly); }
	float& blinkDelay() { return Value<float>(PropOffsets_EditWindow.blinkDelay); }
	float& blinkPeriod() { return Value<float>(PropOffsets_EditWindow.blinkPeriod); }
	float& blinkStart() { return Value<float>(PropOffsets_EditWindow.blinkStart); }
	//DynamicArray& bufferList() { return Value<DynamicArray>(PropOffsets_EditWindow.bufferList); }
	int& currentUndo() { return Value<int>(PropOffsets_EditWindow.currentUndo); }
	USound*& deleteSound() { return Value<USound*>(PropOffsets_EditWindow.deleteSound); }
	float& dragDelay() { return Value<float>(PropOffsets_EditWindow.dragDelay); }
	UTexture*& editCursor() { return Value<UTexture*>(PropOffsets_EditWindow.editCursor); }
	Color& editCursorColor() { return Value<Color>(PropOffsets_EditWindow.editCursorColor); }
	UTexture*& editCursorShadow() { return Value<UTexture*>(PropOffsets_EditWindow.editCursorShadow); }
	USound*& enterSound() { return Value<USound*>(PropOffsets_EditWindow.enterSound); }
	Color& insertColor() { return Value<Color>(PropOffsets_EditWindow.insertColor); }
	float& insertHeight() { return Value<float>(PropOffsets_EditWindow.insertHeight); }
	int& insertHookPos() { return Value<int>(PropOffsets_EditWindow.insertHookPos); }
	int& insertPos() { return Value<int>(PropOffsets_EditWindow.insertPos); }
	float& insertPrefHeight() { return Value<float>(PropOffsets_EditWindow.insertPrefHeight); }
	float& insertPrefWidth() { return Value<float>(PropOffsets_EditWindow.insertPrefWidth); }
	float& insertPreferredCol() { return Value<float>(PropOffsets_EditWindow.insertPreferredCol); }
	UTexture*& insertTexture() { return Value<UTexture*>(PropOffsets_EditWindow.insertTexture); }
	uint8_t& insertType() { return Value<uint8_t>(PropOffsets_EditWindow.insertType); }
	float& insertWidth() { return Value<float>(PropOffsets_EditWindow.insertWidth); }
	float& insertX() { return Value<float>(PropOffsets_EditWindow.insertX); }
	float& insertY() { return Value<float>(PropOffsets_EditWindow.insertY); }
	Color& inverseColor() { return Value<Color>(PropOffsets_EditWindow.inverseColor); }
	float& lastConfigHeight() { return Value<float>(PropOffsets_EditWindow.lastConfigHeight); }
	float& lastConfigWidth() { return Value<float>(PropOffsets_EditWindow.lastConfigWidth); }
	int& maxSize() { return Value<int>(PropOffsets_EditWindow.maxSize); }
	int& maxUndos() { return Value<int>(PropOffsets_EditWindow.maxUndos); }
	USound*& moveSound() { return Value<USound*>(PropOffsets_EditWindow.moveSound); }
	Color& selectColor() { return Value<Color>(PropOffsets_EditWindow.selectColor); }
	int& selectEnd() { return Value<int>(PropOffsets_EditWindow.selectEnd); }
	int& selectEndRow() { return Value<int>(PropOffsets_EditWindow.selectEndRow); }
	float& selectEndX() { return Value<float>(PropOffsets_EditWindow.selectEndX); }
	int& selectStart() { return Value<int>(PropOffsets_EditWindow.selectStart); }
	int& selectStartRow() { return Value<int>(PropOffsets_EditWindow.selectStartRow); }
	float& selectStartX() { return Value<float>(PropOffsets_EditWindow.selectStartX); }
	UTexture*& selectTexture() { return Value<UTexture*>(PropOffsets_EditWindow.selectTexture); }
	float& showAreaHeight() { return Value<float>(PropOffsets_EditWindow.showAreaHeight); }
	float& showAreaWidth() { return Value<float>(PropOffsets_EditWindow.showAreaWidth); }
	float& showAreaX() { return Value<float>(PropOffsets_EditWindow.showAreaX); }
	float& showAreaY() { return Value<float>(PropOffsets_EditWindow.showAreaY); }
	USound*& typeSound() { return Value<USound*>(PropOffsets_EditWindow.typeSound); }
	int& unchangedUndo() { return Value<int>(PropOffsets_EditWindow.unchangedUndo); }

private:
	void DispatchTextChanged(bool modified);
	void ReplaceText(ScriptString newText, bool bUndo, bool bSelect);

	const Array<TextRow>& Rows();
	float RowsTop(int count);
	float RowLeft(float rowWidth);
	float CharToPixel(UGC* gc, const TextRow& row, int col);
	int PixelToChar(UGC* gc, const TextRow& row, float x);
	int YToRow(float pointY);
	int XToCol(int row, float pointX);
	float RowToY(int row);
	float ColToX(int row, int col);
	int RowColToPos(int row, int col);
	void PosToRowCol(int pos, int& row, int& col);
	int XYToPos(float pointX, float pointY);
	void PosToXY(int pos, float& pointX, float& pointY);

	void ComputeCursorConfig(bool bSetPrefCol);
	void ShowCursor();
	void ComputeSelectArea();
	void SetCursorPoint(int pos, bool bDrag);
	void SetCursorRow(int row, bool bDrag);
	int StartOfWord(int pos);
	int EndOfWord(int pos);
	int NextWord(int pos);
	int PrevWord(int pos);
	void DrawRow(UGC* gc, const TextRow& row, float x, float y, const Color& color, const Color& selectedColor, float selectedFrom, float selectedTo);

	// The rows, made again when the text, the wrap width or the fonts change.
	Array<TextRow> rows;
	bool rowsMade = false;
	ScriptString rowsText;
	float rowsWrap = 0.0f;
	UFont* rowsNormalFont = nullptr;
	UFont* rowsBoldFont = nullptr;
	bool rowsSpecialText = false;
};
