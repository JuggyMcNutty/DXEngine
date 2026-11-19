#pragma once

#include <string>
#include <list>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <memory>

class File;

void LogMessage(const std::string& message);
void LogUnimplemented(const std::string& message);

struct LogMessageLine
{
	float Time = 0.0f;
	std::string Source;
	std::string Text;
};

class Logger
{
public:
	static Logger* Get();

	void LogMessage(const std::string& message);
	void LogUnimplemented(const std::string& message);
	void SetTimeSeconds(float value) { time = value; }
	void SetCallback(std::function<void(const LogMessageLine& line)> cb) { std::lock_guard<std::mutex> lock(mutex); printLogDebugger = std::move(cb); }
	// The lines kept, copied under the log's lock: another thread may log
	// meanwhile
	std::list<LogMessageLine> GetLog() const;

	void SaveLog(const std::string& filename) const;
	void SaveLogAsPlaintext(const std::string& filename) const;
	// The last run's log, DXE-Log-LastRun.txt in the engine's folder. Begun
	// as the engine starts, with the lines so far, each line after is added
	// to it and handed to the system at once, as the original writes its log
	// from its first line: a crash leaves the run's log up to it, not the run
	// before's. A flood's lines past StreamLines are left out until
	// SaveLastRun, which writes the file whole, its head and its tail, as the
	// run ends; false when the folder will not take it (a full disk, an I/O
	// error, no folder known), which loses the log and nothing else.
	void BeginLastRun();
	bool SaveLastRun();
	static std::string LastRunFilename();
	static std::list<LogMessageLine> LoadLog(const std::string& filename);

private:
	void Add(LogMessageLine line);

	// The lines kept: the session's first HeadLines and its last TailLines,
	// those between dropped and counted, so that a long session's log keeps
	// its start and its end in bounded memory (bugs.md ENG.33)
	static constexpr size_t HeadLines = 1000;
	static constexpr size_t TailLines = 100000;

	std::atomic<float> time = 0.0f;
	mutable std::mutex mutex;
	std::thread::id mainThread = std::this_thread::get_id();
	std::list<LogMessageLine> Log;
	std::list<LogMessageLine>::iterator TailStart; // the first line past the head, once there is one
	size_t DroppedLines = 0;
	static constexpr size_t StreamLines = HeadLines + TailLines;
	std::shared_ptr<File> LastRun; // the last run's log while lines are added to it
	size_t LastRunLines = 0;
	void StreamLine(const LogMessageLine& line);
	static std::string PlainLine(const LogMessageLine& line);
	std::function<void(const LogMessageLine& line)> printLogDebugger;
	static std::vector<std::string> SplitNewlines(const std::string& str);

	Logger() = default;
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
};
