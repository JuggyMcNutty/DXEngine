
#include "Precomp.h"
#include "Utils/Logger.h"
#include "Utils/JsonValue.h"
#include "Utils/File.h"
#include "VM/Frame.h"
#include <unordered_set>

void LogMessage(const std::string& message)
{
	Logger::Get()->LogMessage(message);
}

void LogUnimplemented(const std::string& message)
{
	Logger::Get()->LogUnimplemented(message);
}

void Logger::LogMessage(const std::string& message)
{
	// The VM's call stack is the main thread's: a line from another thread
	// (a driver's own, a validation layer's) takes no source from it, and the
	// log itself is written under a lock
	bool onMainThread = std::this_thread::get_id() == mainThread;
	std::lock_guard<std::mutex> lock(mutex);
	if (onMainThread && !Frame::Callstack.empty() && Frame::Callstack.back()->Func)
	{
		std::string name;

		if (Frame::Callstack.size() > 1)
		{
			UStruct* func = Frame::Callstack[Frame::Callstack.size() - 2]->Func;
			for (UStruct* s = func; s != nullptr; s = s->StructParent)
			{
				if (name.empty())
					name = s->Name.ToString();
				else
					name = s->Name.ToString() + "." + name;
			}
		}
		else
		{
			name = UObject::GetUClassFullName(Frame::Callstack.front()->Object).ToString();
		}

		for (const std::string& text : SplitNewlines(message))
		{
			LogMessageLine line;
			line.Time = time;
			line.Source = name;
			line.Text = text;
			Add(std::move(line));
		}
	}
	else
	{
		for (const std::string& text : SplitNewlines(message))
		{
			LogMessageLine line;
			line.Time = time;
			line.Text = text;
			Add(std::move(line));
		}
	}
}

// A line onto the log, under its lock: past the head and a full tail, the
// oldest of the tail goes
void Logger::Add(LogMessageLine line)
{
	Log.push_back(std::move(line));
	if (LastRun)
		StreamLine(Log.back());
	if (printLogDebugger)
		printLogDebugger(Log.back());
	if (Log.size() == HeadLines + 1)
		TailStart = std::prev(Log.end());
	else if (Log.size() > HeadLines + TailLines)
	{
		TailStart = Log.erase(TailStart);
		DroppedLines++;
	}
}

std::list<LogMessageLine> Logger::GetLog() const
{
	std::lock_guard<std::mutex> lock(mutex);
	std::list<LogMessageLine> log = Log;
	if (DroppedLines > 0)
	{
		LogMessageLine gap;
		gap.Time = std::next(Log.begin(), HeadLines)->Time;
		gap.Text = "(" + std::to_string(DroppedLines) + (DroppedLines == 1 ? " line" : " lines") + " not kept)";
		log.insert(std::next(log.begin(), HeadLines), std::move(gap));
	}
	return log;
}

void Logger::LogUnimplemented(const std::string& message)
{
	static std::unordered_set<std::string> seenMessages;
	static std::mutex seenMutex;
	bool first;
	{
		std::lock_guard<std::mutex> lock(seenMutex);
		first = seenMessages.insert(message).second;
	}
	if (first)
		LogMessage("Unimplemented: " + message);
}

std::vector<std::string> Logger::SplitNewlines(const std::string& str)
{
	std::vector<std::string> lines;
	size_t pos = 0;
	for (size_t i = 0, count = str.size(); i < count; i++)
	{
		if (str[i] == '\n')
		{
			lines.push_back(str.substr(pos, i - pos));
			if (!lines.back().empty() && lines.back().back() == '\r')
				lines.back().pop_back();
			pos = i + 1;
		}
	}
	lines.push_back(str.substr(pos));
	if (!lines.back().empty() && lines.back().back() == '\r')
		lines.back().pop_back();
	return lines;
}

Logger* Logger::Get()
{
	static Logger logger;
	return &logger;
}

void Logger::SaveLog(const std::string& filename) const
{
	// We want this formatted in a specific way so its reasonably readable both by humans and still by a json parser.
	std::string json = "[";
	bool firstLine = true;
	for (const LogMessageLine& line : GetLog())
	{
		if (!firstLine)
			json += ',';
		firstLine = false;
		json += '\n';
		JsonValue logMessage = JsonValue::array();
		logMessage.items().push_back(JsonValue::number(line.Time));
		logMessage.items().push_back(JsonValue::string(line.Source));
		logMessage.items().push_back(JsonValue::string(line.Text));
		json += logMessage.to_json(false);
	}
	json += "\n]\n";
	File::write_all_text(filename, json);
}

std::string Logger::PlainLine(const LogMessageLine& line)
{
	const auto source = line.Source.empty() ? "<DXEngine>" : line.Source;
	return "[" + std::to_string(line.Time) + "] " + source + ": " + line.Text + "\n";
}

void Logger::SaveLogAsPlaintext(const std::string& filename) const
{
	std::string finalLog;

	for (const LogMessageLine& line : GetLog())
		finalLog += PlainLine(line);

	File::write_all_text(filename, finalLog);
}

// A line onto the last run's log, under the log's lock. A file that will
// not take it ends the adding, not the run.
void Logger::StreamLine(const LogMessageLine& line)
{
	if (LastRunLines >= StreamLines)
		return;
	try
	{
		std::string text = PlainLine(line);
		LastRun->write(text.data(), text.size());
		LastRun->flush();
		LastRunLines++;
	}
	catch (...)
	{
		LastRun.reset();
	}
}

void Logger::BeginLastRun()
{
	std::lock_guard<std::mutex> lock(mutex);
	try
	{
		LastRun = File::create_always(LastRunFilename());
		LastRunLines = 0;
		for (const LogMessageLine& line : Log)
		{
			if (!LastRun)
				break;
			StreamLine(line);
		}
	}
	catch (...)
	{
		LastRun.reset();
	}
}


std::string Logger::LastRunFilename()
{
	return (Directory::engineUserDir() / "DXE-Log-LastRun.txt").string();
}

bool Logger::SaveLastRun()
{
	try
	{
		// The file is let go first: it is written anew, whole
		{
			std::lock_guard<std::mutex> lock(mutex);
			LastRun.reset();
		}
		SaveLogAsPlaintext(LastRunFilename());
		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

std::list<LogMessageLine> Logger::LoadLog(const std::string& filename)
{
	std::list<LogMessageLine> log;
	JsonValue json = JsonValue::parse(File::read_all_text(filename));
	for (const JsonValue& logMessage : json.items())
	{
		LogMessageLine line;
		line.Time = logMessage.at(0).to_float();
		line.Source = logMessage.at(1).to_string();
		line.Text = logMessage.at(2).to_string();
		log.push_back(std::move(line));
	}
	return log;
}
