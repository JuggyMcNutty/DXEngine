
#include "Precomp.h"
#include "ULargeTextWindow.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Engine.h"

// The original's XLargeTextWindow::Init (0x1002d480): lines 1 apart, its
// text not its accelerator.
void ULargeTextWindow::InitDefaults()
{
	UTextWindow::InitDefaults();
	vSpace() = 1.0f;
	lineHeight() = 0.0f;
	EnableTextAsAccelerator(false);
}

void ULargeTextWindow::SetVerticalSpacing(std::optional<float> newVSpace)
{
	// UNUSED from scripts.
	LogUnimplemented("LargeTextWindow.SetVerticalSpacing");
}

// The rows of the window's text wrapped at a width (0 for none), as the GC
// breaks them, and the widest (XLargeTextWindow::GenerateLines 0x1002da70).
int ULargeTextWindow::CountRows(UGC* gc, float wrap, float& widest)
{
	int rows = 0;
	widest = 0.0f;
	UGC::TextState state;
	const char16_t* text = Text().data();
	const char16_t* end = text + Text().size();
	while (true)
	{
		float lineWidth = 0.0f;
		int lineLength = 0;
		if (!gc->GetLine(state, wrap, text, end, lineWidth, lineLength))
			break;
		widest = std::max(widest, lineWidth);
		rows++;
	}
	return rows;
}

// The original's (XLargeTextWindow::ParentRequestedPreferredSize 0x1002e1d0,
// GetTextExtent 0x1002e130): its rows each the font's height, at least 1
// (ComputeLineHeight 0x1002d9d0), its vertical spacing between them and none
// after the last -- an empty text one row high -- as wide as the widest,
// wrapped at the width given less the margins with word wrap on. With no
// height given, a line limit its rows pass (as laid out at its own width)
// holds it to that many rows of the font's height and spacing, its vertical
// spacing between. Then the margins; a width not given at least the minimum.
void ULargeTextWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);

	float wrap = (bWidthSpecified && bWordWrap()) ? std::max(preferredWidth - (hMargin() + hMargin()), 0.0f) : 0.0f;
	// The row height is the script's lineHeight too (the original's
	// ComputeLineHeight 0x1002d9d0 sets it for every large text window)
	float rowHeight = std::max(gc->GetFontHeight(false), 1.0f);
	lineHeight() = rowHeight;
	float widest = 0.0f;
	int rows = CountRows(gc, wrap, widest);
	preferredWidth = widest;
	preferredHeight = rows > 0 ? rows * rowHeight + (rows - 1) * vSpace() : rowHeight;

	if (!bHeightSpecified && (minLines() >= 0 || MaxLines() >= 0))
	{
		float fontHeight = gc->GetFontHeight(true);
		float ownWidest = 0.0f;
		int ownRows = CountRows(gc, bWordWrap() ? Width() - (hMargin() + hMargin()) : 0.0f, ownWidest);
		if (minLines() >= 0 && minLines() > ownRows)
			preferredHeight = (minLines() - 1) * vSpace() + minLines() * fontHeight;
		if (MaxLines() >= 0 && ownRows > MaxLines())
			preferredHeight = (MaxLines() - 1) * vSpace() + MaxLines() * fontHeight;
	}

	preferredWidth += hMargin() + hMargin();
	preferredHeight += vMargin() + vMargin();
	if (!bWidthSpecified && preferredWidth < MinWidth())
		preferredWidth = MinWidth();

	gc->RestoreTextSettings(saved);
}

// The original's XLargeTextWindow::Draw (0x1002e500): the script's
// DrawWindow first, handed the window's GC as it is -- left and top aligned
// and word wrapped, as XGC::XGC (0x10026ad0) makes it, where a text
// window's Draw sets its own first -- then the rows its clip rectangle
// reaches, each drawn on its own within the margins with the GC as the
// script left it (XGC::DrawTextLine 0x10028470): the rows as the window's
// own fonts break them (ConfigurationChanged 0x1002e430), each the font's
// height (at least 1, ComputeLineHeight 0x1002d9d0) whatever its characters,
// its vertical spacing between and none after the last; the block placed by
// the window's vertical alignment -- at the top margin, above the bottom
// one, or centred by the whole half of the room, margins and all -- each
// row across by its horizontal one.
void ULargeTextWindow::DrawWindow(UGC* gc)
{
	UWindow::DrawWindow(gc);
	if (!gc->bDrawEnabled())
		return;

	struct Row
	{
		const char16_t* text;
		int length;
		float width;
		UGC::TextState state;
	};
	Array<Row> rows;
	UGC::TextSettings saved = gc->UseTextSettings(this);
	float rowHeight = std::max(gc->GetFontHeight(false), 1.0f);
	float wrap = bWordWrap() ? Width() - (hMargin() + hMargin()) : 0.0f;
	const char16_t* text = Text().data();
	const char16_t* end = text + Text().size();
	UGC::TextState state;
	while (true)
	{
		UGC::TextState rowState = state;
		float width = 0.0f;
		int length = 0;
		const char16_t* line = gc->GetLine(state, wrap, text, end, width, length);
		if (!line)
			break;
		rows.push_back({ line, length, width, rowState });
	}
	gc->RestoreTextSettings(saved);

	int count = (int)rows.size();
	float block = count * rowHeight + (count > 1 ? (count - 1) * vSpace() : 0.0f);
	float top;
	switch ((EVAlign)VAlign())
	{
	case EVAlign::Top: top = vMargin(); break;
	case EVAlign::Bottom: top = Height() - block - vMargin(); break;
	default: top = (float)(int)((Height() - block) * 0.5f); break;
	}
	float step = rowHeight + vSpace();
	const ClipRect& clip = winClipRect();
	int first = std::max((int)std::floor((clip.clipY + 0.00001f - top) / step), 0);
	int last = std::min((int)std::ceil((clip.clipY + clip.clipHeight - top) / step), count);

	gc->PushClip(gc->ScaleRect(Rectf::xywh(gc->offsetX + hMargin(), gc->offsetY + vMargin(), Width() - (hMargin() + hMargin()), Height() - (vMargin() + vMargin()))));
	float y = top + step * first;
	for (int i = first; i < last; i++, y += step)
	{
		const Row& row = rows[i];
		float x;
		switch ((EHAlign)HAlign())
		{
		case EHAlign::Left: x = hMargin(); break;
		case EHAlign::Right: x = Width() - row.width - hMargin(); break;
		default: x = (float)(int64_t)((Width() - row.width) * 0.5f); break;
		}
		UGC::TextState rowState = row.state;
		const char16_t* p = row.text;
		const char16_t* rowEnd = row.text + row.length;
		while (char16_t ch = gc->GetNextChar(p, rowState, rowEnd))
		{
			UFont* font = rowState.bold ? gc->boldFont() : gc->normalFont();
			if (ch == '\n' || !font)
				continue;
			float height = 0.0f;
			gc->DrawChar(font, ch, rowState.ownColor ? rowState.color : gc->TextColor(), rowState.accel, x, y, x, height);
		}
	}
	gc->PopClip();
}

// The original's (XLargeTextWindow::ParentRequestedGranularity 0x1002e380,
// ComputeLineHeight 0x1002d9d0): a line down -- the font's height, at least
// 1, plus the vertical spacing -- a pixel across; then the script's say.
void ULargeTextWindow::ParentRequestedGranularity(float& hGranularity, float& vGranularity)
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);
	vGranularity = std::max(gc->GetFontHeight(false), 1.0f) + vSpace();
	hGranularity = 1.0f;
	gc->RestoreTextSettings(saved);
	UWindow::ParentRequestedGranularity(hGranularity, vGranularity);
}
