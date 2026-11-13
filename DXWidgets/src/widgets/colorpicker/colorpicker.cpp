#include "widgets/colorpicker/colorpicker.h"
#include "widgets/slider/slider.h"
#include "widgets/lineedit/lineedit.h"
#include "widgets/pushbutton/pushbutton.h"
#include "core/colorf.h"
#include <algorithm>
#include <cstdlib>
#include <string>

ColorPicker::ColorPicker(Widget* parent) : Widget(parent)
{
	SetStyleClass("listview");
	for (int i = 0; i < 3; i++)
	{
		Edits[i] = new LineEdit(this);
		Edits[i]->SetNumericMode(true);
		Edits[i]->SetMaxLength(3);
		Edits[i]->SetText("0");
		Edits[i]->FuncAfterEditChanged = [this, i]() { SetChannel(i, std::atoi(Edits[i]->GetText().c_str()), false); };
		Edits[i]->FuncEnterPressed = [this]() { Accept(); };
		Edits[i]->FuncIgnoreKeyDown = [this](InputKey key) {
			if (key != InputKey::Escape)
				return false;
			Cancel();
			return true;
		};

		Sliders[i] = new Slider(this);
		Sliders[i]->SetMin(0);
		Sliders[i]->SetMax(255);
		Sliders[i]->SetValue(0);
		Sliders[i]->ValueChanged = [this, i](int value) { SetChannel(i, value, true); };
	}
	OkButton = new PushButton(this);
	OkButton->SetText("OK");
	OkButton->OnClick = [this]() { Accept(); };
	CancelButton = new PushButton(this);
	CancelButton->SetText("Cancel");
	CancelButton->OnClick = [this]() { Cancel(); };
}

void ColorPicker::SetColor(int red, int green, int blue)
{
	SetChannel(0, red, false);
	SetChannel(1, green, false);
	SetChannel(2, blue, false);
}

// A channel's value from its slider or its number, the other made to show it
void ColorPicker::SetChannel(int channel, int value, bool fromSlider)
{
	if (Updating)
		return;
	Updating = true;
	value = std::clamp(value, 0, 255);
	Values[channel] = value;
	if (fromSlider)
		Edits[channel]->SetText(std::to_string(value));
	else
	{
		Sliders[channel]->SetValue(value);
		if (Edits[channel]->GetText() != std::to_string(value) && !Edits[channel]->GetText().empty())
			Edits[channel]->SetText(std::to_string(value));
	}
	Updating = false;
	Update();
}

void ColorPicker::Accept()
{
	if (OnAccept)
		OnAccept();
}

void ColorPicker::Cancel()
{
	if (OnCancel)
		OnCancel();
}

void ColorPicker::OnKeyDown(InputKey key)
{
	if (key == InputKey::Escape)
		Cancel();
	else if (key == InputKey::Enter)
		Accept();
}

void ColorPicker::OnGeometryChanged()
{
	double w = GetWidth(), h = GetHeight();
	double row = 26.0;
	for (int i = 0; i < 3; i++)
	{
		double y = 8.0 + i * row;
		Edits[i]->SetFrameGeometry(Rect::xywh(28.0, y, 40.0, 20.0));
		Sliders[i]->SetFrameGeometry(Rect::xywh(74.0, y, std::max(w - 74.0 - 70.0, 20.0), 20.0));
	}
	OkButton->SetFrameGeometry(Rect::xywh(w - 160.0, h - 30.0, 72.0, 24.0));
	CancelButton->SetFrameGeometry(Rect::xywh(w - 80.0, h - 30.0, 72.0, 24.0));
}

void ColorPicker::OnPaint(Canvas* canvas)
{
	PseudoWidget body("listview-body");
	Colorf text = body.GetStyleColor("color");
	auto font = GetFont();
	auto vtp = canvas->verticalTextAlign(font);
	static const char* const labels[] = { "R", "G", "B" };
	for (int i = 0; i < 3; i++)
	{
		double y = 8.0 + i * 26.0;
		canvas->drawText(font, Point(10.0, y + (20.0 - (vtp.bottom - vtp.top)) * 0.5 + vtp.baseline), labels[i], text);
	}
	double w = GetWidth();
	Rect swatch = Rect::xywh(w - 60.0, 8.0, 52.0, 72.0);
	canvas->fillRect(swatch, text);
	canvas->fillRect(Rect::xywh(swatch.x + 1.0, swatch.y + 1.0, swatch.width - 2.0, swatch.height - 2.0), Colorf::fromRgba8((uint8_t)Values[0], (uint8_t)Values[1], (uint8_t)Values[2]));
	std::string colorText = "(R=" + std::to_string(Values[0]) + ",G=" + std::to_string(Values[1]) + ",B=" + std::to_string(Values[2]) + ")";
	canvas->drawText(font, Point(10.0, 8.0 + 3 * 26.0 + vtp.baseline), colorText, text);
}
