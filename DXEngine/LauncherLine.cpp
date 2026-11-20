#include "Precomp.h"
#include "LauncherLine.h"
#include "Utils/Logger.h"
#include <cstdlib>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

LauncherLine& LauncherLine::Get()
{
	static LauncherLine instance;
	return instance;
}

bool LauncherLine::Open()
{
	if (tried)
		return handle >= 0;
	tried = true;
#ifndef _WIN32
	// DXLauncher's mark of a game running (its bugs.md D.95), which this
	// process holds for its whole run, as the original's mutex is the game's:
	// kept from anything it starts in turn, which would hold the mark past
	// the game's end.
	if (const char* mark = std::getenv("DXL_LAUNCHER_MARK_FD"))
	{
		char* markEnd = nullptr;
		long long fd = std::strtoll(mark, &markEnd, 10);
		if (*mark && !*markEnd && fd > 2 && fd <= INT32_MAX && fcntl((int)fd, F_GETFD) >= 0)
			fcntl((int)fd, F_SETFD, FD_CLOEXEC);
		unsetenv("DXL_LAUNCHER_MARK_FD");
	}
#else
	// DXLauncher's marks (its bugs.md D.95 and LPORT.23): handles of its
	// named mutexes, of a launcher running and of a client's game playing,
	// "<mark>" or "<mark>,<playing>", which this process holds for its whole
	// run, as the original's mutex is the game's: kept from anything it
	// starts in turn, which would hold them past the game's end.
	if (const char* mark = std::getenv("DXL_LAUNCHER_MARK_FD"))
	{
		std::string marks = mark;
		for (size_t at = 0; at < marks.size();)
		{
			size_t comma = marks.find(',', at);
			if (comma == std::string::npos)
				comma = marks.size();
			std::string word = marks.substr(at, comma - at);
			char* wordEnd = nullptr;
			long long h = std::strtoll(word.c_str(), &wordEnd, 10);
			DWORD flags = 0;
			if (!word.empty() && !*wordEnd && h > 0 && GetHandleInformation((HANDLE)(intptr_t)h, &flags))
				SetHandleInformation((HANDLE)(intptr_t)h, HANDLE_FLAG_INHERIT, 0);
			at = comma + 1;
		}
		SetEnvironmentVariableA("DXL_LAUNCHER_MARK_FD", nullptr);
	}
#endif
	const char* value = std::getenv("DXL_LAUNCHER_FD");
	if (!value || !*value)
		return false;
	char* end = nullptr;
	long long n = std::strtoll(value, &end, 10);
#ifndef _WIN32
	if (*end || n < 0 || n > INT32_MAX || fcntl((int)n, F_GETFD) < 0)
	{
		LogMessage("Launcher line: no descriptor " + std::string(value));
		return false;
	}
	handle = (intptr_t)n;
	// Not handed on to anything this process starts: neither the descriptor
	// nor its number, which a relaunch of the engine's own would take for a
	// line (a descriptor reused by then)
	fcntl((int)handle, F_SETFD, FD_CLOEXEC);
	unsetenv("DXL_LAUNCHER_FD");
	fcntl((int)handle, F_SETFL, fcntl((int)handle, F_GETFL) | O_NONBLOCK);
#else
	// The client end of the launcher's pipe, which it waits on: read only
	// what has come (PeekNamedPipe), so the game's loop never waits on it.
	if (*end || n <= 0 || GetFileType((HANDLE)(intptr_t)n) != FILE_TYPE_PIPE)
	{
		LogMessage("Launcher line: no pipe " + std::string(value));
		return false;
	}
	handle = (intptr_t)n;
	// Not handed on to anything this process starts: neither the handle nor
	// its number, which a relaunch of the engine's own would take for a line
	SetHandleInformation((HANDLE)handle, HANDLE_FLAG_INHERIT, 0);
	SetEnvironmentVariableA("DXL_LAUNCHER_FD", nullptr);
#endif
	return true;
}

void LauncherLine::Send(const std::string& line)
{
	if (!Open())
		return;
	std::string data = line + "\n";
#ifndef _WIN32
	if (send((int)handle, data.data(), data.size(), MSG_NOSIGNAL) < 0)
		LogMessage("Launcher line: the launcher is gone");
#else
	DWORD wrote = 0;
	if (!WriteFile((HANDLE)handle, data.data(), (DWORD)data.size(), &wrote, nullptr))
		LogMessage("Launcher line: the launcher is gone");
#endif
}

void LauncherLine::Hello()
{
	Send("hello");
}

void LauncherLine::Ready()
{
	Send("ready");
}

void LauncherLine::Relaunch(const std::string& commandLine)
{
	Send("relaunch " + commandLine);
}

void LauncherLine::Poll(const std::function<void(const std::string&)>& onLine)
{
	if (!Open())
		return;
	char buffer[1024];
	for (;;)
	{
#ifndef _WIN32
		ssize_t n = recv((int)handle, buffer, sizeof(buffer), MSG_DONTWAIT);
		if (n <= 0)
		{
			if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
			{
				// The launcher has gone: nothing more will come.
				close((int)handle);
				handle = -1;
			}
			break;
		}
#else
		DWORD avail = 0, n = 0;
		if (!PeekNamedPipe((HANDLE)handle, nullptr, 0, nullptr, &avail, nullptr) ||
			(avail && !ReadFile((HANDLE)handle, buffer, avail < sizeof(buffer) ? avail : (DWORD)sizeof(buffer), &n, nullptr)))
		{
			// The launcher has gone: nothing more will come.
			CloseHandle((HANDLE)handle);
			handle = -1;
			break;
		}
		if (n == 0)
			break;
#endif
		pending.append(buffer, (size_t)n);
	}
	size_t end;
	while ((end = pending.find('\n')) != std::string::npos)
	{
		std::string line = pending.substr(0, end);
		pending.erase(0, end + 1);
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (!line.empty())
			onLine(line);
	}
}
