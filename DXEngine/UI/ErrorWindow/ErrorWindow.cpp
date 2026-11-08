
#include "Precomp.h"
#include "ErrorWindow.h"
#include "Utils/File.h"
#include "Utils/CrashReporter.h"
#include "Utils/CommandLine.h"
#include "Utils/ScriptString.h"
#include "Engine.h"
#include "LauncherLine.h"
#include <dxwidgets/core/image.h>
#include <dxwidgets/core/timer.h>
#include <dxwidgets/widgets/pushbutton/pushbutton.h>
#include <dxwidgets/widgets/scrollbar/scrollbar.h>
#include <dxwidgets/systemdialogs/save_file_dialog.h>
#include <miniz.h>

bool ErrorWindow::CheckCrashReporter()
{
#ifdef _MSC_VER
	// The crash reporter's relaunch to show its report, MSVC's alone, as the
	// reporter is (ENGINE.md): no other build writes a report to show
	if (commandline->HasArg("", "--showcrashreport") && commandline->GetItems().size() == 2)
	{
		std::string dumpFilename = commandline->GetItems()[0];
		std::string logFilename = commandline->GetItems()[1];
		try
		{
			CrashDumpInfo dumpInfo = CrashReporter::GetCrashDumpInfo(dumpFilename);
			std::list<LogMessageLine> log = Logger::Get()->LoadLog(logFilename);

			LogMessageLine line;
			line.Source = "Crash Reporter";
			line.Text = dumpInfo.exception;
			log.push_back(line);

			size_t pos = 0;
			while (pos < dumpInfo.callstack.size())
			{
				size_t end = std::min(dumpInfo.callstack.find('\n', pos), dumpInfo.callstack.size());
				line.Text = dumpInfo.callstack.substr(pos, end - pos);
				log.push_back(line);
				if (end == dumpInfo.callstack.size())
					break;
				pos = end + 1;
			}
			ExecModal(dumpInfo.exception, log, File::read_all_bytes(dumpFilename));

			File::try_delete(dumpFilename);
			File::try_delete(logFilename);
		}
		catch (...)
		{
		}
		return true;
	}
#endif
	// The engine's own folder, where its last run's log goes; the reports'
	// folder in it only where a report is written, MSVC's build, every other
	// build's reporter being empty (bugs.md ENG.38)
	Directory::create(Directory::engineUserDir().string());
#ifdef _MSC_VER
	const auto reportsDirectory = Directory::engineUserDir() / "CrashReports";
	Directory::create(reportsDirectory.string());
	CrashReporter::Init(reportsDirectory.string(), [](const std::string& logFilename) { Logger::Get()->SaveLog(logFilename); });
#endif
	return false;
}

void ErrorWindow::ExecModal(const std::string& text, const std::list<LogMessageLine>& log, Array<uint8_t> minidump, const std::vector<std::shared_ptr<Image>>& icon)
{
	// 1500x800 DIPs, or less to fit the work area of the screen it opens on,
	// centred there with room round it for the caption and frame, which lie
	// outside the client area set here
	Rect area = DisplayBackend::Get()->GetScreenWorkArea();
	double windowWidth = 1500.0;
	double windowHeight = 800.0;
	if (area.width > 0.0 && area.height > 0.0)
	{
		windowWidth = std::min(windowWidth, area.width - 40.0);
		windowHeight = std::min(windowHeight, area.height - 80.0);
	}

	auto window = std::make_unique<ErrorWindow>(std::move(minidump), icon);
	window->SetText(text, log);
	window->SetFrameGeometry(area.x + (area.width - windowWidth) * 0.5, area.y + (area.height - windowHeight) * 0.5, windowWidth, windowHeight);
	window->Show();

	// A stop as the game takes one closes the window as its Close does: a
	// signal on POSIX (ENGINE.md, running it), or the launcher's Quit, the
	// stop where there are no signals, which the frame loop alone read
	Timer stopTimer(window.get());
	stopTimer.FuncExpired = []() {
		bool stop = Engine::IsStopSignalled();
		LauncherLine::Get().Poll([&](const std::string& line) { stop = stop || line == "Quit"; });
		if (stop)
			DisplayWindow::ExitLoop();
	};
	stopTimer.Start(100, true);

	DisplayWindow::RunLoop();
}

ErrorWindow::ErrorWindow(Array<uint8_t> initminidump, const std::vector<std::shared_ptr<Image>>& icon) : Widget(nullptr, WidgetType::Window), minidump(std::move(initminidump))
{
	SetWindowTitle("Fatal Error - DXEngine");
	SetWindowBackground(Colorf::fromRgba8(51, 51, 51));
	SetWindowBorderColor(Colorf::fromRgba8(51, 51, 51));
	SetWindowCaptionColor(Colorf::fromRgba8(33, 33, 33));
	SetWindowCaptionTextColor(Colorf::fromRgba8(226, 223, 219));
	if (!icon.empty())
		SetWindowIcon(icon);

	LogView = new LogViewer(this);
	ClipboardButton = new PushButton(this);
	ClipboardButton->OnClick = [this]() { OnClipboardButtonClicked(); };
	ClipboardButton->SetText("Copy to clipboard");

	if (minidump.empty())
	{
		CloseButton = new PushButton(this);
		CloseButton->OnClick = [this]() { OnCloseButtonClicked(); };
		CloseButton->SetText("Close");
	}
	else
	{
		SaveReportButton = new PushButton(this);
		SaveReportButton->OnClick = [this]() { OnSaveReportButtonClicked(); };
		SaveReportButton->SetText("Save Report");
	}

	LogView->SetFocus();
}


// The engine's text is UTF-8, as the widgets and the clipboard take it
// (Utils/ScriptString.h): it goes as it is, a byte that is not UTF-8 as the
// character of its value, as the engine reads such text
static std::string WidgetText(const std::string& text)
{
	return ScriptToUTF8(UTF8ToScript(text));
}

void ErrorWindow::SetText(const std::string& text, const std::list<LogMessageLine>& log)
{
	LogView->SetText(text, log);

	clipboardtext.clear();
	for (const LogMessageLine& line : log)
	{
		std::string timeText = ToFixed(line.Time);
		if (timeText.size() < 10)
			timeText.resize(10, ' ');

		std::string sourceText = line.Source;
		if (sourceText.size() < 50)
			sourceText.resize(50, ' ');

		clipboardtext += timeText;
		clipboardtext += " ";
		clipboardtext += WidgetText(sourceText);
		clipboardtext += " ";
		clipboardtext += WidgetText(line.Text);
		clipboardtext += "\n";
	}
	clipboardtext += "\nExecution could not continue.\n";
	clipboardtext += WidgetText(text);
	clipboardtext += "\n";
}

std::string ErrorWindow::ToFixed(float time)
{
	std::string fixedTime = std::to_string((int64_t)(time * 1000.0));
	if (fixedTime.size() < 4)
		fixedTime.insert(0, 4 - fixedTime.size(), '0'); // padded on the left: 5 ms is 0.005, not 5.000
	return fixedTime.substr(0, fixedTime.size() - 3) + "." + fixedTime.substr(fixedTime.size() - 3);
}

void ErrorWindow::OnClipboardButtonClicked()
{
	SetClipboardText(clipboardtext);
}

void ErrorWindow::OnCloseButtonClicked()
{
	DisplayWindow::ExitLoop();
}

void ErrorWindow::OnSaveReportButtonClicked()
{
	auto dialog = SaveFileDialog::Create(this);
	dialog->AddFilter("Crash Report Zip Files", "*.zip");
	dialog->AddFilter("All Files", "*.*");
	dialog->SetFilename("CrashReport.zip");
	dialog->SetDefaultExtension("zip");
	if (dialog->Show())
	{
		std::string filename = dialog->Filename();

		mz_zip_archive zip = {};
		if (mz_zip_writer_init_heap(&zip, 0, 16 * 1024 * 1024))
		{
			mz_zip_writer_add_mem(&zip, "minidump.dmp", minidump.data(), minidump.size(), MZ_DEFAULT_COMPRESSION);
			mz_zip_writer_add_mem(&zip, "log.txt", clipboardtext.data(), clipboardtext.size(), MZ_DEFAULT_COMPRESSION);
		}
		void* buffer = nullptr;
		size_t buffersize = 0;
		mz_zip_writer_finalize_heap_archive(&zip, &buffer, &buffersize);
		mz_zip_writer_end(&zip);

		try
		{
			File::write_all_bytes(filename, buffer, buffersize);
		}
		catch (...)
		{
		}
	}
}

void ErrorWindow::OnClose()
{
	DisplayWindow::ExitLoop();
}

// Esc closes the window as its close box does, whatever has the focus (the
// log, or a button clicked), as a dialog's Esc cancels it
void ErrorWindow::OnWindowKeyDown(InputKey key)
{
	if (key == InputKey::Escape)
		OnClose();
	else
		Widget::OnWindowKeyDown(key);
}

void ErrorWindow::OnGeometryChanged()
{
	double w = GetWidth();

	double buttonheight = ClipboardButton->GetPreferredHeight() + ClipboardButton->GetNoncontentTop() + ClipboardButton->GetNoncontentBottom();
	double y = GetHeight() - 15.0 - buttonheight;
	ClipboardButton->SetFrameGeometry(20.0, y, 170.0, buttonheight);
	if (CloseButton)
		CloseButton->SetFrameGeometry(GetWidth() - 20.0 - 100.0, y, 100.0, buttonheight);
	else if (SaveReportButton)
		SaveReportButton->SetFrameGeometry(GetWidth() - 20.0 - 100.0, y, 100.0, buttonheight);
	y -= 20.0;

	LogView->SetFrameGeometry(Rect::xywh(0.0, 0.0, w, y));
}

/////////////////////////////////////////////////////////////////////////////

LogViewer::LogViewer(Widget* parent) : Widget(parent)
{
	SetNoncontentSizes(8.0, 8.0, 3.0, 8.0);

	scrollbar = new Scrollbar(this);
	scrollbar->FuncScroll = [this]() { OnScrollbarScroll(); };
}

void LogViewer::SetText(const std::string& text, const std::list<LogMessageLine>&log)
{
	errorlines.clear();
	loglines.clear();

	for (const LogMessageLine& line : log)
	{
		loglines.push_back(CreateLineLayout(line));
	}

	SpanLayout layout;

	// Add an empty line as a bit of spacing
	layout.AddText(" ", largefont, Colorf::fromRgba8(255, 255, 170));
	errorlines.push_back(layout);

	layout.Clear();
	//layout.AddImage(Image::LoadResource("widgets/erroricon.svg"), -8.0);
	layout.AddText("Execution could not continue.", largefont, Colorf::fromRgba8(255, 170, 170));
	errorlines.push_back(layout);

	size_t pos = 0;
	size_t end = text.size();
	while (pos < end)
	{
		size_t linestart = pos;
		size_t lineend = std::min(text.find('\n', linestart), end);
		std::string line = text.substr(linestart, lineend - linestart);
		pos = lineend + 1;
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty())
			line.push_back(' ');
		line = WidgetText(line);

		layout.Clear();
		layout.AddText(line, font, Colorf::fromRgba8(255, 255, 170));
		errorlines.push_back(layout);
	}

	scrollbar->SetRanges(0.0, (double)(loglines.size() + errorlines.size()), 1.0, 100.0);
	scrollbar->SetPosition((double)(loglines.size() + errorlines.size()) - 1.0);

	Update();
}

std::string LogViewer::ToFixed(float time)
{
	std::string fixedTime = std::to_string((int64_t)(time * 1000.0));
	if (fixedTime.size() < 4)
		fixedTime.insert(0, 4 - fixedTime.size(), '0'); // padded on the left: 5 ms is 0.005, not 5.000
	return fixedTime.substr(0, fixedTime.size() - 3) + "." + fixedTime.substr(fixedTime.size() - 3);
}

LogViewer::LogLine LogViewer::CreateLineLayout(const LogMessageLine& line)
{
	LogLine logline;
	logline.time.AddText(ToFixed(line.Time), font, Colorf::fromRgba8(200, 200, 200));
	logline.source.AddText(WidgetText(line.Source), font, Colorf::fromRgba8(170, 170, 255));
	logline.text.AddText(WidgetText(line.Text), font, Colorf::fromRgba8(255, 255, 255));
	return logline;
}

void LogViewer::OnPaintFrame(Canvas* canvas)
{
	double w = GetFrameGeometry().width;
	double h = GetFrameGeometry().height;
	canvas->fillRect(Rect::xywh(0.0, 0.0, w, h), Colorf::fromRgba8(38, 38, 38));
	//canvas->fillRect(Rect::xywh(0.0, 0.0, w, 1.0), bordercolor);
	//canvas->fillRect(Rect::xywh(0.0, h - 1.0, w, 1.0), bordercolor);
	//canvas->fillRect(Rect::xywh(0.0, 0.0, 1.0, h - 0.0), bordercolor);
	//canvas->fillRect(Rect::xywh(w - 1.0, 0.0, 1.0, h - 0.0), bordercolor);
}

void LogViewer::OnPaint(Canvas* canvas)
{
	double width = GetWidth() - scrollbar->GetFrameGeometry().width;
	double y = GetHeight();
	size_t start = std::min((size_t)std::round(scrollbar->GetPosition() + 1.0), errorlines.size() + loglines.size());
	for (size_t i = start; i > 0 && y > 0.0; i--)
	{
		if (i <= loglines.size())
		{
			LogLine& line = loglines[i - 1];

			line.text.Layout(canvas, width - 500.0);
			line.time.Layout(canvas, 100.0);
			line.source.Layout(canvas, 400.0);

			double lineheight = line.time.GetSize().height;
			lineheight = std::max(lineheight, line.source.GetSize().height);
			lineheight = std::max(lineheight, line.text.GetSize().height);
			y -= lineheight;

			line.time.SetPosition(Point(0.0, y));
			line.source.SetPosition(Point(100.0, y));
			line.text.SetPosition(Point(500.0, y));

			line.time.DrawLayout(canvas);
			line.source.DrawLayout(canvas);
			line.text.DrawLayout(canvas);
		}
		else
		{
			SpanLayout& layout = errorlines[i - loglines.size() - 1];
			layout.Layout(canvas, width);
			layout.SetPosition(Point(0.0, y - layout.GetSize().height));
			layout.DrawLayout(canvas);
			y -= layout.GetSize().height;
		}
	}
}

bool LogViewer::OnMouseWheel(const Point& pos, InputKey key)
{
	if (key == InputKey::MouseWheelUp)
	{
		ScrollUp(4);
	}
	else if (key == InputKey::MouseWheelDown)
	{
		ScrollDown(4);
	}
	return true;
}

void LogViewer::OnKeyDown(InputKey key)
{
	if (key == InputKey::Home)
	{
		scrollbar->SetPosition(0.0f);
		Update();
	}
	if (key == InputKey::End)
	{
		scrollbar->SetPosition(scrollbar->GetMax());
		Update();
	}
	else if (key == InputKey::PageUp)
	{
		ScrollUp(20);
	}
	else if (key == InputKey::PageDown)
	{
		ScrollDown(20);
	}
	else if (key == InputKey::Up)
	{
		ScrollUp(4);
	}
	else if (key == InputKey::Down)
	{
		ScrollDown(4);
	}
}

void LogViewer::OnScrollbarScroll()
{
	Update();
}

void LogViewer::OnGeometryChanged()
{
	double w = GetWidth();
	double h = GetHeight();
	double sw = scrollbar->GetPreferredWidth();
	scrollbar->SetFrameGeometry(Rect::xywh(w - sw, 0.0, sw, h));
}

void LogViewer::ScrollUp(int lines)
{
	scrollbar->SetPosition(std::max(scrollbar->GetPosition() - (double)lines, 0.0));
	Update();
}

void LogViewer::ScrollDown(int lines)
{
	scrollbar->SetPosition(std::min(scrollbar->GetPosition() + (double)lines, scrollbar->GetMax()));
	Update();
}
