#pragma once

#include "../../core/widget.h"
#include <functional>

class Slider;
class LineEdit;
class PushButton;

// A colour chosen by its red, green and blue, each 0 to 255 by a slider or
// its number, the colour shown beside them with its text, and OK and Cancel:
// a panel its owner places over its own widgets, taking the clicks on it,
// until OK (Enter in a number) or Cancel (Escape) is pressed. The owner
// closes it; it must not be deleted in its own callbacks.
class ColorPicker : public Widget
{
public:
	ColorPicker(Widget* parent);

	void SetColor(int red, int green, int blue);
	int GetRed() const { return Values[0]; }
	int GetGreen() const { return Values[1]; }
	int GetBlue() const { return Values[2]; }

	std::function<void()> OnAccept;
	std::function<void()> OnCancel;

	double GetPreferredWidth() override { return 300.0; }
	double GetPreferredHeight() override { return 132.0; }

protected:
	void OnPaint(Canvas* canvas) override;
	void OnGeometryChanged() override;
	bool OnMouseDown(const Point& pos, InputKey key) override { return true; }
	bool OnMouseDoubleclick(const Point& pos, InputKey key) override { return true; }
	bool OnMouseWheel(const Point& pos, InputKey key) override { return true; }
	void OnKeyDown(InputKey key) override;

private:
	void SetChannel(int channel, int value, bool fromSlider);
	void Accept();
	void Cancel();

	int Values[3] = {};
	Slider* Sliders[3] = {};
	LineEdit* Edits[3] = {};
	PushButton* OkButton = nullptr;
	PushButton* CancelButton = nullptr;
	bool Updating = false;
};
