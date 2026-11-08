// wincheck: what a game's window holds of the mouse, and what it does with
// a window command, for the Win32 window's runs (bugs.md WIN.32, WIN.33).
// Run in the game's own Wine prefix (the harness's build/dxcap/prefix), beside
// a run whose game is in play, so the window has locked the mouse:
//
//   wine wincheck.exe clip <title part>
//       the cursor's clip rectangle (GetClipCursor), the window's client
//       rectangle on the screen, the window thread's capture window
//       (GetGUIThreadInfo) and the cursor's place, one line
//   wine wincheck.exe syscmd <title part> screensave|monitorpower
//       WM_SYSCOMMAND SC_SCREENSAVE, or SC_MONITORPOWER with -1 (the display
//       on: nothing is turned off whoever answers), sent to the window
//
// The original's window is titled "Deus Ex", DXEngine's "... - DXEngine".
// Built with the port's llvm-mingw:
//   x86_64-w64-mingw32-clang -O2 -o wincheck.exe wincheck.c -luser32
// (the original is 32-bit; a 64-bit helper reads and sends to its window as
// to any other, the clip and the capture being the desktop's and the
// thread's).
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HWND Found;
static const char* Part;

static BOOL CALLBACK EachWindow(HWND hwnd, LPARAM lparam)
{
	char title[256] = {};
	if (!IsWindowVisible(hwnd))
		return TRUE;
	GetWindowTextA(hwnd, title, sizeof(title));
	if (strstr(title, Part))
	{
		Found = hwnd;
		return FALSE;
	}
	return TRUE;
}

int main(int argc, char** argv)
{
	if (argc < 3 || (strcmp(argv[1], "clip") && strcmp(argv[1], "syscmd")) || (!strcmp(argv[1], "syscmd") && argc < 4))
	{
		fprintf(stderr, "usage: wincheck clip <title part>\n       wincheck syscmd <title part> screensave|monitorpower\n");
		return 2;
	}
	Part = argv[2];
	EnumWindows(EachWindow, 0);
	if (!Found)
	{
		printf("WINCHECK: no window titled *%s*\n", Part);
		return 1;
	}

	if (!strcmp(argv[1], "clip"))
	{
		RECT clip = {}, client = {};
		POINT corner = {}, cursor = {};
		GUITHREADINFO info = {};
		info.cbSize = sizeof(info);
		GetClipCursor(&clip);
		GetClientRect(Found, &client);
		ClientToScreen(Found, &corner);
		GetCursorPos(&cursor);
		DWORD thread = GetWindowThreadProcessId(Found, NULL);
		BOOL gotInfo = GetGUIThreadInfo(thread, &info);
		printf("WINCHECK: clip %ld %ld %ld %ld; client %ld %ld %ld %ld; capture %s; cursor %ld %ld; virtual screen %d %d %d %d\n",
			clip.left, clip.top, clip.right, clip.bottom,
			corner.x, corner.y, corner.x + client.right, corner.y + client.bottom,
			!gotInfo ? "unknown" : info.hwndCapture == Found ? "the window" : info.hwndCapture ? "another window" : "none",
			cursor.x, cursor.y,
			GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
			GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
			GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN));
		return 0;
	}

	WPARAM command = !strcmp(argv[3], "monitorpower") ? SC_MONITORPOWER : SC_SCREENSAVE;
	LPARAM argument = command == SC_MONITORPOWER ? (LPARAM)-1 : 0;
	DWORD_PTR answer = 0;
	LRESULT sent = SendMessageTimeoutA(Found, WM_SYSCOMMAND, command, argument, SMTO_ABORTIFHUNG, 5000, &answer);
	printf("WINCHECK: syscmd %s %s, answer %ld\n", argv[3], sent ? "taken" : "timed out", (long)answer);
	return 0;
}
