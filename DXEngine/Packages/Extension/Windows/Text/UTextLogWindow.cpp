
#include "Precomp.h"
#include "UTextLogWindow.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Engine/Resources/Textures/UTexture.h"
#include "Engine.h"

// The original's XTextLogWindow (Extension.dll): its lines kept, each with
// its colour and the time it is shown, and joined by newlines into the
// window's text (RegenerateText 0x10047dc0), which sizes the window as a
// text window's does; at each configuration, bTooTall if that text is taller
// than the window less its margins (ConfigurationChanged 0x10047910); drawn
// line by line (Draw 0x10047a10), from the newest up when too tall, so the
// oldest leave at the top.
void UTextLogWindow::AddLog(const std::string& NewText, const Color& linecol)
{
	LogEntry entry;
	entry.text = NewText;
	entry.color = linecol;
	entry.timestamp = totalTime + textTimeout();
	logs.push_back(entry);
	RegenerateText();
}

void UTextLogWindow::ClearLog()
{
	logs.clear();
	RegenerateText();
}

// The original's XTextLogWindow::Init (0x100474b0): left and top aligned, a
// 5 s timeout
void UTextLogWindow::InitDefaults()
{
	UTextWindow::InitDefaults();
	HAlign() = (uint8_t)EHAlign::Left;
	VAlign() = (uint8_t)EVAlign::Top;
	textTimeout() = 5.0f;
}

void UTextLogWindow::RegenerateText()
{
	std::string text;
	for (size_t i = 0; i < logs.size(); i++)
	{
		if (i > 0)
			text += "\n";
		text += logs[i].text;
	}
	Text() = UTF8ToScript(text);
	AskParentForReconfigure();
}

void UTextLogWindow::PauseLog(bool bNewPauseState)
{
	bPaused() = bNewPauseState;
}

void UTextLogWindow::SetTextTimeout(float newTimeout)
{
	textTimeout() = newTimeout;
}

void UTextLogWindow::Tick(float timeElapsed)
{
	UTextWindow::Tick(timeElapsed);
	if (!bPaused())
	{
		totalTime += timeElapsed;

		size_t expired = 0;
		while (expired < logs.size() && logs[expired].timestamp <= totalTime)
			expired++;
		if (expired > 0)
		{
			logs.erase(logs.begin(), logs.begin() + expired);
			RegenerateText();
		}
	}
}

void UTextLogWindow::ConfigurationChanged()
{
	UGC* gc = engine->dxgc;
	UGC::TextSettings saved = gc->UseTextSettings(this);
	float xExtent = 0.0f, yExtent = 0.0f;
	gc->GetTextExtent(Width() - (hMargin() + hMargin()), xExtent, yExtent, Text());
	gc->RestoreTextSettings(saved);
	bTooTall() = Height() - (vMargin() + vMargin()) < yExtent;
	UTextWindow::ConfigurationChanged();
}

void UTextLogWindow::DrawWindow(UGC* gc)
{
	UWindow::DrawWindow(gc);

	bool tooTall = bTooTall();
	float w = Width() - (hMargin() + hMargin());
	float h = Height() - (vMargin() + vMargin());

	gc->SetAlignments(HAlign(), tooTall ? (uint8_t)EVAlign::Bottom : VAlign());
	gc->EnableWordWrap(true);

	float y = vMargin();
	for (size_t n = 0; n < logs.size(); n++)
	{
		const LogEntry& line = logs[tooTall ? logs.size() - 1 - n : n];
		float xExtent = 0.0f, yExtent = 0.0f;
		gc->GetTextExtent(w, xExtent, yExtent, line.text);
		gc->SetTextColor(line.color);
		gc->DrawText(hMargin(), y, w, h, line.text);
		y += tooTall ? -yExtent : yExtent;
	}
}
