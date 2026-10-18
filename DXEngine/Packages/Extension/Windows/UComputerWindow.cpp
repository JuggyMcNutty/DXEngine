
#include "Precomp.h"
#include "UComputerWindow.h"
#include "Engine.h"
#include "Packages/Engine/Resources/UFont.h"
#include "Packages/Extension/Windows/UGC.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Extension/Windows/Text/UEditWindow.h"
#include "VM/ScriptCall.h"
#include <algorithm>

void UComputerWindow::ClearLine(int rowToClear)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.ClearLine");
}

void UComputerWindow::FadeOutText(std::optional<float> fadeDuration)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.FadeOutText");
}

void UComputerWindow::GetChar(const std::string& inputKey, std::optional<bool> bEcho)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.GetChar");
}

void UComputerWindow::GetInput(int MaxLength, const std::string& inputKey, std::optional<std::string> defaultInputString, std::optional<std::string> inputMask)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.GetInput");
}

float UComputerWindow::GetThrottle()
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.GetThrottle");
	return 0.0f;
}

bool UComputerWindow::IsBufferFlushed()
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.IsBufferFlushed");
	return false;
}

bool UComputerWindow::IsPaused()
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.IsPaused");
	return false;
}

void UComputerWindow::Pause(std::optional<float> pauseLength)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.Pause");
}

void UComputerWindow::PlaySoundLater(UObject* newsound)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.PlaySoundLater");
}

void UComputerWindow::PrintGraphic(UObject* Graphic, int Width, int Height, std::optional<int> posX, std::optional<int> posY, std::optional<bool> bStatic, std::optional<bool> bPixelPos)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.PrintGraphic");
}

void UComputerWindow::PrintLn()
{
	QueuedItem item;
	item.Type = ItemType::NewLine;
	Queue(item, true);
}

void UComputerWindow::ResetThrottle()
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.ResetThrottle");
}

void UComputerWindow::Resume()
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.Resume");
}

void UComputerWindow::SetBackgroundTextures(UObject* backTexture1, UObject* backTexture2, UObject* backTexture3, UObject* backTexture4, UObject* backTexture5, UObject* backTexture6)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetBackgroundTextures");
}

void UComputerWindow::SetComputerSoundVolume(float newSoundVolume)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetComputerSoundVolume");
}

void UComputerWindow::SetCursorBlinkSpeed(float newBlinkSpeed)
{
	// UNUSED from scripts.
	cursorBlinkSpeed() = newBlinkSpeed;
}

void UComputerWindow::SetCursorTexture(UObject* newCursorTexture, std::optional<int> newCursorWidth, std::optional<int> newCursorHeight)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetCursorTexture");
}

void UComputerWindow::SetFontColor(const Color& newFontColor)
{
	QueuedItem item;
	item.Type = ItemType::Colour;
	item.Colour = newFontColor;
	Queue(item, false);
}

void UComputerWindow::SetTextPosition(int posX, int posY)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetTextPosition");
}

void UComputerWindow::SetTextSound(UObject* newTextSound)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetTextSound");
}

void UComputerWindow::SetTextWindowPosition(int newX, int newY)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetTextWindowPosition");
}

void UComputerWindow::SetThrottle(float throttleModifier)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetThrottle");
}

void UComputerWindow::SetTypingSound(UObject* newTypingSound)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetTypingSound");
}

void UComputerWindow::SetTypingSoundVolume(float newSoundVolume)
{
	// UNUSED from scripts.
	LogUnimplemented("ComputerWindow.SetTypingSoundVolume");
}

// The script's clear waits its turn in the queue (XComputerWindow
// 0x10011610), so a speech stays up until the next starts typing.
void UComputerWindow::ClearScreen()
{
	QueuedItem item;
	item.Type = ItemType::Clear;
	Queue(item, false);
}

void UComputerWindow::EnableWordWrap(std::optional<bool> bNewWordWrap)
{
	bWordWrap() = bNewWordWrap.value_or(true);
}

// Each character queued a time step after the last (0x10011810), a ^ a
// line break; then a line break unless bNewLine is False.
void UComputerWindow::Print(const std::string& printText, std::optional<bool> bNewLine)
{
	for (char c : printText)
	{
		QueuedItem item;
		item.Type = c == '^' ? ItemType::NewLine : ItemType::Text;
		item.Char = c;
		Queue(item, true);
	}
	if (bNewLine.value_or(true))
		PrintLn();
}

void UComputerWindow::SetCursorColor(const Color& newCursorColor)
{
	cursorColor() = newCursorColor;
}

void UComputerWindow::SetFadeSpeed(float newFadeSpeed)
{
	fadeSpeed() = newFadeSpeed;
}

// The font and the cell at once, the window sized to the grid; the colour
// queued, for the characters typed after it (0x10010e70).
void UComputerWindow::SetTextFont(UObject* NewFont, int newFontWidth, int newFontHeight, const Color& newFontColor)
{
	textFont() = UObject::Cast<UFont>(NewFont);
	fontWidth() = newFontWidth;
	fontHeight() = newFontHeight;
	FitToText();
	SetFontColor(newFontColor);
}

void UComputerWindow::SetTextSize(int newCols, int newRows)
{
	textCols() = newCols;
	textRows() = newRows;
	FitToText();
}

void UComputerWindow::FitToText()
{
	if (textFont())
		SetSize((float)(textCols() * fontWidth()), (float)(textRows() * fontHeight()));
}

void UComputerWindow::SetTextTiming(float newTiming)
{
	eventTimeInterval() = newTiming;
}

void UComputerWindow::ShowTextCursor(std::optional<bool> bShow)
{
	bShowCursor() = !bShow.has_value() ? true : bShow.value();
}

// The original's XComputerWindow::Init (0x1000d530): selectable, its first
// tick passed over, the blink's count started; one made while the game is
// paused goes on through pauses. The rest is the class's defaults.
void UComputerWindow::InitDefaults()
{
	UWindow::InitDefaults();
	bIsSelectable() = true;
	bFirstTick() = true;
	cursorNextEvent() = cursorBlinkSpeed();
	bIgnoreGamePaused() = engine->LevelInfo && !engine->LevelInfo->Pauser().empty();
}

void UComputerWindow::ParentRequestedPreferredSize(bool bWidthSpecified, float& preferredWidth, bool bHeightSpecified, float& preferredHeight)
{
	if (textFont())
	{
		if (!bWidthSpecified)
			preferredWidth = (float)(textCols() * fontWidth());
		if (!bHeightSpecified)
			preferredHeight = (float)(textRows() * fontHeight());
	}

	UWindow::ParentRequestedPreferredSize(bWidthSpecified, preferredWidth, bHeightSpecified, preferredHeight);
}

// Every add stamped with the next event's time, which a character or line
// break moves on by the interval (eventTimeInterval).
void UComputerWindow::Queue(QueuedItem item, bool advance)
{
	item.Time = timeNextEvent();
	if (advance)
		timeNextEvent() += eventTimeInterval();
	queued.push_back(item);
	timeLastEvent() = timeCurrent();
}

// The original's Tick (0x1000f7b0), by the windows' real time: the cursor's
// blink, then the queue's clock (ProcessQueuedBuffer, 0x1000fe20), whose
// due items run in order, then the typed characters' fade-in.
void UComputerWindow::Tick(float timeElapsed)
{
	UWindow::Tick(timeElapsed);

	if (bIgnoreTick())
		return;
	if (bFirstTick())
	{
		bFirstTick() = false;
		return;
	}
	bGamePaused() = engine->LevelInfo && !engine->LevelInfo->Pauser().empty() && !bIgnoreGamePaused();
	if (bGamePaused())
		return;

	if (!bComputerStartInvoked() && bIsVisible())
	{
		bComputerStartInvoked() = true;
		if (FindEventFunction(this, "ComputerStart"))
			CallEvent(this, "ComputerStart");
	}

	if (cursorBlinkSpeed() > 0.0f)
	{
		cursorNextEvent() -= timeElapsed;
		if (cursorNextEvent() < 0.0f)
		{
			cursorNextEvent() += cursorBlinkSpeed();
			bCursorVisible() = !bCursorVisible();
		}
	}

	timeCurrent() += timeElapsed * throttle();
	if (timeCurrent() - timeLastEvent() > eventTimeInterval())
		timeNextEvent() = timeCurrent() + eventTimeInterval();
	ProcessQueue();

	if (fadeSpeed() > 0.0f)
	{
		for (TypedChar& c : typed)
			c.FadeTimer = std::max(c.FadeTimer - timeElapsed, 0.0f);
	}
}

void UComputerWindow::ProcessQueue()
{
	size_t i = 0;
	for (; i < queued.size() && queued[i].Time < timeCurrent(); i++)
	{
		const QueuedItem& item = queued[i];
		switch (item.Type)
		{
		case ItemType::Text: TypeChar(i); break;
		case ItemType::NewLine: NewRow(); break;
		case ItemType::Colour: FontColor() = item.Colour; break;
		case ItemType::Clear: typed.clear(); TextX() = 0; TextY() = 0; break;
		}
	}
	queued.erase(queued.begin(), queued.begin() + i);
}

// One character typed (CalculateCharDisplayPosition, 0x10010710): the
// cursor lit and its blink started again; the last column never used; with
// word wrap a space at a row's start dropped, and a word that would reach
// the last column started on the next row whole.
void UComputerWindow::TypeChar(size_t index)
{
	const QueuedItem& item = queued[index];
	bCursorVisible() = true;
	cursorNextEvent() = cursorBlinkSpeed();

	int cols = textCols();
	if (item.Char == ' ')
	{
		if (TextX() >= cols - 1)
		{
			NewRow();
			return;
		}
		if (bWordWrap() && TextX() == 0)
			return;
	}
	else if (bWordWrap())
	{
		int count = 1;
		for (size_t j = index + 1; j < queued.size() && queued[j].Type == ItemType::Text && queued[j].Char != ' '; j++)
			count++;
		if (TextX() + count >= cols)
			NewRow();
	}
	else if (TextX() >= cols - 1)
	{
		NewRow();
	}

	TypedChar c;
	c.Char = item.Char;
	c.X = TextX();
	c.Y = TextY();
	c.Colour = FontColor();
	c.FadeTimer = fadeSpeed();
	typed.push_back(c);
	TextX()++;
}

// The next row's start; past the last row, every typed character moves up
// one and the top row's go (Scroll, 0x1000eaf0).
void UComputerWindow::NewRow()
{
	TextX() = 0;
	TextY()++;
	if (TextY() >= textRows())
	{
		TextY() = std::max(textRows() - 1, 0);
		for (TypedChar& c : typed)
			c.Y--;
		typed.erase(std::remove_if(typed.begin(), typed.end(), [](const TypedChar& c) { return c.Y < 0; }), typed.end());
	}
}

// The original's Draw (0x1000ed60, DrawDisplayBuffer 0x1000fae0): the
// script's DrawWindow, then each typed character alone in its cell, in its
// own colour faded in from black over fadeSpeed, then the cursor -- its
// texture as a bar under the next cell, in the cursor's colour, while lit
// and no input field shows.
void UComputerWindow::DrawWindow(UGC* gc)
{
	UWindow::DrawWindow(gc);

	if (UFont* font = textFont())
	{
		gc->SetFonts(font, font);
		gc->bWordWrap() = false;
		for (const TypedChar& c : typed)
		{
			Color colour = c.Colour;
			if (fadeSpeed() > 0.0f)
			{
				float f = (fadeSpeed() - c.FadeTimer) / fadeSpeed();
				colour.R = (uint8_t)(colour.R * f);
				colour.G = (uint8_t)(colour.G * f);
				colour.B = (uint8_t)(colour.B * f);
			}
			gc->SetTextColor(colour);
			gc->DrawText((float)(c.X * fontWidth()), (float)(c.Y * fontHeight()), (float)fontWidth(), (float)fontHeight(), std::string(1, c.Char));
		}
	}

	bool inputShown = inputWindow() && inputWindow()->IsVisible(true);
	if (cursorTexture() && bShowCursor() && bCursorVisible() && !inputShown)
	{
		gc->SetStyle(EDrawStyle::Masked);
		gc->SetTileColor(cursorColor());
		gc->DrawPattern((float)(fontWidth() * TextX()), (float)(fontHeight() * (TextY() + 1) - cursorHeight()), (float)cursorWidth(), (float)cursorHeight(), 0.0f, 0.0f, cursorTexture());
	}
}
