#include "Precomp.h"
#include "ServerConsole.h"

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <poll.h>
#include <unistd.h>
#else
#include <mutex>
#include <thread>
#include "Utils/UTF16.h"
#endif

ServerConsole& ServerConsole::Get()
{
	static ServerConsole instance;
	return instance;
}

#ifdef _WIN32
namespace
{
	// What the reading thread hands over: the text read, and whether the
	// input has ended. Made once and never freed: the thread is still in its
	// read as the process exits.
	struct ServerInput
	{
		std::mutex Lock;
		std::string Text;
		bool Ended = false;
	};
	ServerInput* Input = nullptr;
}
#endif

void ServerConsole::Start()
{
	started = true;
#ifndef _WIN32
	// A background job's read of its terminal fails, where it would stop the
	// server
	std::signal(SIGTTIN, SIG_IGN);
#else
	HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
	if (!handle || handle == INVALID_HANDLE_VALUE)
	{
		ended = true;
		return;
	}
	Input = new ServerInput();
	ServerInput* input = Input;
	DWORD mode = 0;
	bool console = GetFileType(handle) == FILE_TYPE_CHAR && GetConsoleMode(handle, &mode);
	std::thread([handle, console, input]()
	{
		for (;;)
		{
			std::string text;
			if (console)
			{
				// The console's own characters, UTF-16, a line at a time
				wchar_t buffer[512];
				DWORD count = 0;
				if (!ReadConsoleW(handle, buffer, 512, &count, nullptr))
					break;
				text = from_utf16(std::wstring(buffer, count));
			}
			else
			{
				char buffer[1024];
				DWORD count = 0;
				if (!ReadFile(handle, buffer, sizeof(buffer), &count, nullptr) || count == 0)
					break;
				text.assign(buffer, count);
			}
			std::lock_guard<std::mutex> guard(input->Lock);
			input->Text += text;
		}
		std::lock_guard<std::mutex> guard(input->Lock);
		input->Ended = true;
	}).detach();
#endif
}

void ServerConsole::Poll(const std::function<void(const std::string&)>& onLine)
{
	if (!started)
		Start();
	if (!ended)
	{
#ifndef _WIN32
		for (;;)
		{
			struct pollfd fd = { STDIN_FILENO, POLLIN, 0 };
			if (poll(&fd, 1, 0) <= 0)
				break;
			if (fd.revents & POLLNVAL)
			{
				ended = true;
				break;
			}
			char buffer[1024];
			ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
			if (count > 0)
			{
				pending.append(buffer, (size_t)count);
				continue;
			}
			// Nothing for now -- a signal, or a background job's terminal --
			// or the input's end
			if (count < 0 && (errno == EINTR || errno == EAGAIN || errno == EIO))
				break;
			ended = true;
			break;
		}
#else
		std::lock_guard<std::mutex> guard(Input->Lock);
		pending += Input->Text;
		Input->Text.clear();
		ended = Input->Ended;
#endif
	}
	size_t end;
	while ((end = pending.find('\n')) != std::string::npos)
	{
		std::string line = pending.substr(0, end);
		pending.erase(0, end + 1);
		onLine(line);
	}
	// The input's end ends its last line
	if (ended && !pending.empty())
	{
		std::string line;
		line.swap(pending);
		onLine(line);
	}
}
