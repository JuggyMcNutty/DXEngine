#pragma once

#include "UTextWindow.h"

class UTextLogWindow : public UTextWindow
{
public:
	using UTextWindow::UTextWindow;

	void InitDefaults() override;
	void AddLog(const std::string& NewText, const Color& linecol);
	void ClearLog();
	void PauseLog(bool bNewPauseState);
	void SetTextTimeout(float newTimeout);

	void Tick(float timeElapsed) override;
	void ConfigurationChanged() override;
	void DrawWindow(UGC* gc) override;

	//DynamicArray& Lines() { return Value<DynamicArray>(PropOffsets_TextLogWindow.Lines); }
	BitfieldBool bPaused() { return BoolValue(PropOffsets_TextLogWindow.bPaused); }
	BitfieldBool bTooTall() { return BoolValue(PropOffsets_TextLogWindow.bTooTall); }
	float& textTimeout() { return Value<float>(PropOffsets_TextLogWindow.textTimeout); }

private:
	struct LogEntry
	{
		std::string text;
		Color color;
		float timeLeft = 0.0f;
	};
	void RegenerateText();

	Array<LogEntry> logs;
};
