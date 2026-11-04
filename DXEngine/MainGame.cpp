
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
				args.push_back(from_utf16(argv[i]));
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

int main(int argc, char** argv)
{
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
