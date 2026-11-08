#pragma once

#include <string>
#include <list>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

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
	std::function<void(const LogMessageLine& line)> printLogDebugger;
	static std::vector<std::string> SplitNewlines(const std::string& str);

	Logger() = default;
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
};
