#pragma once

#include <functional>
#include <string>

// A dedicated server's typed commands (bugs.md D.65): the lines of its
// standard input, which the engine runs as the original's log window runs a
// typed line (Engine::RunTypedLine). The input's end -- a closed pipe,
// /dev/null, Ctrl-D -- is ignored, the server running on, a last line with
// no line break at it taken as a line. On POSIX the frame's poll reads what
// has come and never waits, SIGTTIN ignored so that a server in a shell's
// background reads nothing until it is brought to the front, where its read
// would have stopped it; on Windows a thread reads the console or the pipe
// given, its lines handed over under a lock. Without a standard input every
// call does nothing.
class ServerConsole
{
public:
	static ServerConsole& Get();

	// Each whole line come in since the last call, to onLine; never waits.
	void Poll(const std::function<void(const std::string&)>& onLine);

private:
	void Start();

	bool started = false;
	bool ended = false;
	std::string pending;
};
