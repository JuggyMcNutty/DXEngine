
#include "Precomp.h"
#include "GameApp.h"
#include "Utils/UTF16.h"
#include <iostream>
#include <vector>
#ifdef WIN32
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>
#include <timeapi.h>
#include <cstdio>
#include <cwctype>
#include <fcntl.h>
#include <io.h>
#endif

#ifdef WIN32

#ifdef _MSC_VER
#pragma comment(linker, "\"/manifestdependency:type='Win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Ws2_32.lib")
#endif

class InitCOM
{
public:
	InitCOM()
	{
		HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (FAILED(result))
			Exception::Throw("CoInitializeEx(COINIT_APARTMENTTHREADED) failed");
	}

	~InitCOM()
	{
		CoUninitialize();
	}
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
	try
	{
		Array<std::string> args;
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (argv)
		{
			for (int i = 1; i < argc; i++)
			{
				// A quoted folder ending in a backslash ("C:\Deus Ex\", as a
				// batch file's %~dp0 gives it) is read as an escaped quote, the
				// line's rest in the same word. No Windows path holds a quote:
				// a word not an option is cut at one before a blank, its
				// backslash given back, and the rest read again (bugs.md ENG.47)
				std::wstring arg = argv[i];
				size_t quote = arg.find(L'"');
				if (arg[0] != L'-' && quote != std::wstring::npos && (quote + 1 == arg.size() || iswspace(arg[quote + 1])))
				{
					args.push_back(from_utf16(arg.substr(0, quote) + L"\\"));
					int restc = 0;
					LPWSTR* rest = CommandLineToArgvW((L"x " + arg.substr(quote + 1)).c_str(), &restc);
					if (rest)
					{
						for (int j = 1; j < restc; j++)
							args.push_back(from_utf16(rest[j]));
						LocalFree(rest);
					}
					continue;
				}
				args.push_back(from_utf16(argv[i]));
			}
			LocalFree(argv);
		}

		//SetProcessDPIAware();
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
		InitCommonControls();

		WORD winsock_version = MAKEWORD(2, 2);
		WSADATA wsaData;
		int err = WSAStartup(winsock_version, &wsaData);
		if (err != 0)
			Exception::Throw("Failed to initialize winsockets");

		InitCOM initCOM;

		// A windowed program is given no console: one started at a console's
		// prompt with no output of its own (a launcher's pipe, a file) writes
		// it to that console, --help, --version, the log and an error; one
		// with neither shows an error that comes before its error window can
		// open in a message box (GameApp::main; bugs.md ENG.46)
		auto nowhere = [](DWORD which) { HANDLE handle = GetStdHandle(which); return handle == nullptr || handle == INVALID_HANDLE_VALUE; };
		bool outputNowhere = nowhere(STD_OUTPUT_HANDLE);
		bool errorNowhere = nowhere(STD_ERROR_HANDLE);
		if ((outputNowhere || errorNowhere) && AttachConsole(ATTACH_PARENT_PROCESS))
		{
			if (outputNowhere)
				freopen("CONOUT$", "w", stdout);
			if (errorNowhere && freopen("CONOUT$", "w", stderr))
				errorNowhere = false;
		}

		// The log as Linux' engine writes it, its lines ended by \n alone,
		// where a launcher gave the engine its output.
		if (_fileno(stdout) >= 0)
			_setmode(_fileno(stdout), _O_BINARY);
		if (_fileno(stderr) >= 0)
			_setmode(_fileno(stderr), _O_BINARY);

		// Waits as long as they ask -- a frame's, the mixer's 2 ms -- where
		// Windows' timer otherwise ticks every 15.6 ms.
		timeBeginPeriod(1);
		GameApp app;
		app.ErrorBox = errorNowhere;
		int exitcode = app.main(std::move(args));
		timeEndPeriod(1);
		return exitcode;
	}
	catch (const std::exception& e)
	{
		MessageBoxW(0, to_utf16(e.what()).c_str(), to_utf16("Unhandled Exception").c_str(), MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}
}

#else

#include <csignal>

// A write to a pipe whose reader is gone (the log on stderr through a pipe,
// its reader closed) fails, where SIGPIPE would kill the engine before it
// writes its inis back: a handler that does nothing, not SIG_IGN, so that a
// program the engine starts gets the default again (bugs.md ENG.45)
static void OnBrokenPipe(int)
{
}

int main(int argc, char** argv)
{
	struct sigaction brokenPipe = {};
	brokenPipe.sa_handler = OnBrokenPipe;
	sigemptyset(&brokenPipe.sa_mask);
	sigaction(SIGPIPE, &brokenPipe, nullptr);

	try
	{
		Array<std::string> args;
		for (int i = 1; i < argc; i++)
			args.push_back(argv[i]);

		GameApp app;
		return app.main(std::move(args));
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
		return 1;
	}
}

#endif
