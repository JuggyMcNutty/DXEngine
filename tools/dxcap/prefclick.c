// prefclick: the original's property windows walked and edited as a player
// would, for PrefsConsole's and EditActorConsole's runs (bugs.md D.81,
// WIN.38): PREFERENCES' Advanced Options and EDITACTOR's inspectors. Under
// Wine uinput keys and X clicks do not reach them; messages from a program in
// the same prefix do.
// Run in the game's own Wine prefix (the harness's build/dxcap/prefix), beside
// a run whose window is open. The window is found by its caption,
// PREFCLICK_TITLE's, "Advanced Options" without it (an inspector's is its
// object's class's name and " Properties": "JCDentonMale Properties").
// Its tree is one owner-drawn list box, a row of it named by its index there
// (count gives the rows). A click on a row's box -- 8 px in, 12 px further at
// each depth below the top items' -- opens or closes the row and selects it;
// one on a leaf selects it and shows its editor; an array opens by a double
// click on its box.
//
//   wine prefclick.exe toplevels
//       every visible top-level window: handle, caption, class, rectangle
//   wine prefclick.exe list
//       the window's visible children: class, id, text, place in the
//       client area, size, style; a combo's choices and its selection
//   wine prefclick.exe count
//       the tree's rows, its top row, its selection, a row's height
//   wine prefclick.exe settop <row>
//       the tree scrolled to show <row> first
//   wine prefclick.exe walk <row> [<depth>]
//       the row opened and everything under it, depth first, each leaf
//       selected and the editor it shows listed, each array opened
//   wine prefclick.exe toggle <row> <depth>
//       a click on the row's box, the rows counted before and after
//   wine prefclick.exe select <row> <depth>
//       a click on the row's box, the editor and buttons it shows listed
//   wine prefclick.exe type <row> <depth> <text> [noenter]
//       the leaf selected, its edit box's text all selected, <text> typed,
//       then Enter, but with noenter (another row's select then leaves it)
//   wine prefclick.exe key <row> <depth> <virtual key>
//       the row selected by a click on its name, then the key (0x28 Down)
//       pressed and let go, to the tree
//   wine prefclick.exe button <text>
//       a click on the visible button of that text: Reset To Defaults, Add,
//       Empty, Insert, Delete
//   wine prefclick.exe post <x> <y> [dbl]
//       a click, or a double click, posted to the window under that point
//       of the client area
//   wine prefclick.exe close
//       the window closed, as by its close box (WM_CLOSE)
//
// No combo is picked: under Wine the original's combo list opens to no click
// SendInput puts in, and a pick's messages sent to the combo and its parent do
// not commit.
// Built with the port's llvm-mingw:
//   x86_64-w64-mingw32-clang -O2 -o prefclick.exe prefclick.c -luser32
// (the original is 32-bit; a 64-bit helper reads its windows and sends to them
// as to any other).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HWND Top, Tree, Found;
static const char* WantClass;
static const WCHAR* WantText;
static int SawAdd;

static BOOL CALLBACK FindTree(HWND hwnd, LPARAM lparam)
{
	char cls[128] = {};
	GetClassNameA(hwnd, cls, sizeof(cls));
	if (strstr(cls, "ItemBox") || !_stricmp(cls, "ListBox"))
	{
		Tree = hwnd;
		return FALSE;
	}
	return TRUE;
}

// A window's class, id, text, place in the client area, size and style; a
// combo's choices and its selection.
static void Describe(HWND hwnd, const char* prefix)
{
	char cls[128] = {}, text[512] = {};
	WCHAR wide[512] = {};
	RECT rect = {};
	GetClassNameA(hwnd, cls, sizeof(cls));
	SendMessageW(hwnd, WM_GETTEXT, 512, (LPARAM)wide);
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, text, sizeof(text), NULL, NULL);
	GetWindowRect(hwnd, &rect);
	POINT corner = { rect.left, rect.top };
	ScreenToClient(Top, &corner);
	printf("%s'%s' id %ld text '%s' at %ld,%ld size %ldx%ld style %08lx\n", prefix, cls, (long)GetDlgCtrlID(hwnd), text,
		corner.x, corner.y, rect.right - rect.left, rect.bottom - rect.top, (unsigned long)GetWindowLongA(hwnd, GWL_STYLE));
	if (!strstr(cls, "ComboBox") && !strstr(cls, "COMBOBOX"))
		return;
	LRESULT count = SendMessageA(hwnd, CB_GETCOUNT, 0, 0), selected = SendMessageA(hwnd, CB_GETCURSEL, 0, 0);
	printf("%s  combo: %ld choices, selected %ld:", prefix, (long)count, (long)selected);
	for (LRESULT i = 0; i < count && i < 400; i++)
	{
		WCHAR item[512] = {};
		char utf8[1024] = {};
		if (SendMessageW(hwnd, CB_GETLBTEXTLEN, (WPARAM)i, 0) < 511)
			SendMessageW(hwnd, CB_GETLBTEXT, (WPARAM)i, (LPARAM)item);
		WideCharToMultiByte(CP_UTF8, 0, item, -1, utf8, sizeof(utf8), NULL, NULL);
		printf(" [%s]", utf8);
	}
	printf("\n");
}

// Each visible child but the tree described; an "Add" among them marks the
// selected row an array.
static BOOL CALLBACK EachVisible(HWND hwnd, LPARAM prefix)
{
	if (hwnd == Tree || !IsWindowVisible(hwnd))
		return TRUE;
	WCHAR text[64] = {};
	GetWindowTextW(hwnd, text, 64);
	if (!lstrcmpW(text, L"Add"))
		SawAdd = 1;
	Describe(hwnd, (const char*)prefix);
	return TRUE;
}

static void ListVisible(const char* prefix)
{
	SawAdd = 0;
	EnumChildWindows(Top, EachVisible, (LPARAM)prefix);
}

static BOOL CALLBACK FindVisible(HWND hwnd, LPARAM lparam)
{
	char cls[128] = {};
	WCHAR text[128] = {};
	if (!IsWindowVisible(hwnd))
		return TRUE;
	GetClassNameA(hwnd, cls, sizeof(cls));
	GetWindowTextW(hwnd, text, 128);
	if ((!WantClass || strstr(cls, WantClass)) && (!WantText || !lstrcmpW(text, WantText)))
		Found = hwnd;
	return TRUE; // on to the last one: the newest editor
}

// The last visible child whose class holds cls and whose text is text (NULL
// for any text).
static HWND Visible(const char* cls, const WCHAR* text)
{
	Found = NULL;
	WantClass = cls;
	WantText = text;
	EnumChildWindows(Top, FindVisible, 0);
	return Found;
}

static int Rows(void)
{
	return (int)SendMessageA(Tree, LB_GETCOUNT, 0, 0);
}

// A click on the row, x px in from its left, the tree scrolled to show it
// first; then a double click with dbl. The pause lets what the window posts
// itself after a click run: an editor's making, a resize.
static void ClickRow(int row, int x, int dbl)
{
	RECT rect = {};
	SendMessageA(Tree, LB_SETTOPINDEX, (WPARAM)row, 0);
	SendMessageA(Tree, LB_GETITEMRECT, (WPARAM)row, (LPARAM)&rect);
	LPARAM at = MAKELPARAM(rect.left + x, (rect.top + rect.bottom) / 2);
	SendMessageA(Tree, WM_LBUTTONDOWN, MK_LBUTTON, at);
	SendMessageA(Tree, WM_LBUTTONUP, 0, at);
	if (dbl)
	{
		SendMessageA(Tree, WM_LBUTTONDBLCLK, MK_LBUTTON, at);
		SendMessageA(Tree, WM_LBUTTONUP, 0, at);
	}
	Sleep(150);
}

static int BoxX(int depth)
{
	return 8 + 12 * depth;
}

// The row opened and everything under it, depth first; the rows it and they
// take, opened.
static int Walk(int row, int depth)
{
	int before = Rows();
	ClickRow(row, BoxX(depth), 0);
	int added = Rows() - before;
	if (added <= 0)
	{
		printf("row %d depth %d: leaf%s\n", row, depth, added < 0 ? " (it closed)" : "");
		ListVisible("    editor ");
		if (!SawAdd)
			return 1;
		ClickRow(row, BoxX(depth), 1);
		added = Rows() - before;
		if (added <= 0)
		{
			printf("row %d depth %d: an array, not opened\n", row, depth);
			return 1;
		}
		printf("row %d depth %d: an array, opened by a double click, %d under it\n", row, depth, added);
	}
	else
	{
		printf("row %d depth %d: opened, %d under it\n", row, depth, added);
		ListVisible("    shown ");
	}
	int next = row + 1;
	for (int i = 0; i < added; i++)
		next += Walk(next, depth + 1);
	return next - row;
}

static int Usage(void)
{
	fprintf(stderr, "usage: prefclick toplevels | list | count | settop <row> | walk <row> [<depth>]\n"
		"       prefclick toggle|select <row> <depth> | type <row> <depth> <text> [noenter]\n"
		"       prefclick key <row> <depth> <virtual key> | button <text> | post <x> <y> [dbl] | close\n"
		"(PREFCLICK_TITLE: the window's caption, \"Advanced Options\" without it)\n");
	return 2;
}

int main(int argc, char** argv)
{
	if (argc < 2)
		return Usage();
	const char* command = argv[1];

	if (!strcmp(command, "toplevels"))
	{
		HWND hwnd = NULL;
		while ((hwnd = FindWindowExA(NULL, hwnd, NULL, NULL)) != NULL)
		{
			if (!IsWindowVisible(hwnd))
				continue;
			char cls[128] = {}, text[256] = {};
			RECT rect = {};
			GetClassNameA(hwnd, cls, sizeof(cls));
			GetWindowTextA(hwnd, text, sizeof(text));
			GetWindowRect(hwnd, &rect);
			printf("top %p '%s' class '%s' at %ld,%ld size %ldx%ld\n", (void*)hwnd, text, cls, rect.left, rect.top,
				rect.right - rect.left, rect.bottom - rect.top);
		}
		return 0;
	}

	const char* title = getenv("PREFCLICK_TITLE");
	Top = FindWindowA(NULL, title && *title ? title : "Advanced Options");
	if (!Top)
	{
		printf("PREFCLICK: no window\n");
		return 1;
	}
	EnumChildWindows(Top, FindTree, 0);
	RECT window = {}, client = {}, tree = {};
	GetWindowRect(Top, &window);
	GetClientRect(Top, &client);
	if (Tree)
		GetClientRect(Tree, &tree);
	printf("PREFCLICK: window %p at %ld,%ld size %ldx%ld, client %ldx%ld; tree %p client %ldx%ld\n", (void*)Top,
		window.left, window.top, window.right - window.left, window.bottom - window.top, client.right, client.bottom,
		(void*)Tree, tree.right, tree.bottom);

	if (!strcmp(command, "list"))
	{
		ListVisible("child ");
		return 0;
	}

	if (!strcmp(command, "close"))
	{
		SendMessageA(Top, WM_CLOSE, 0, 0);
		Sleep(300);
		printf("PREFCLICK: closed; the window %s\n",
			IsWindow(Top) ? (IsWindowVisible(Top) ? "stays, shown" : "stays, hidden") : "is gone");
		return 0;
	}

	if (!strcmp(command, "button") && argc >= 3)
	{
		WCHAR text[128] = {};
		MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, text, 128);
		HWND button = Visible("Button", text);
		if (!button)
		{
			printf("PREFCLICK: no button '%s'\n", argv[2]);
			return 1;
		}
		SendMessageA(button, BM_CLICK, 0, 0);
		Sleep(300);
		printf("PREFCLICK: clicked '%s'\n", argv[2]);
		return 0;
	}

	if (!strcmp(command, "post") && argc >= 4)
	{
		int dbl = argc >= 5 && !strcmp(argv[4], "dbl");
		POINT screen = { atoi(argv[2]), atoi(argv[3]) };
		ClientToScreen(Top, &screen);
		HWND child = Top;
		for (;;)
		{
			POINT at = screen;
			ScreenToClient(child, &at);
			HWND under = ChildWindowFromPointEx(child, at, CWP_SKIPINVISIBLE);
			if (!under || under == child)
				break;
			child = under;
		}
		POINT at = screen;
		ScreenToClient(child, &at);
		LPARAM lparam = MAKELPARAM(at.x, at.y);
		PostMessageA(child, WM_LBUTTONDOWN, MK_LBUTTON, lparam);
		PostMessageA(child, WM_LBUTTONUP, 0, lparam);
		if (dbl)
		{
			PostMessageA(child, WM_LBUTTONDBLCLK, MK_LBUTTON, lparam);
			PostMessageA(child, WM_LBUTTONUP, 0, lparam);
		}
		printf("PREFCLICK: posted to %p at %ld,%ld%s\n", (void*)child, at.x, at.y, dbl ? " (double)" : "");
		return 0;
	}

	if (!Tree)
	{
		printf("PREFCLICK: no tree\n");
		return 1;
	}

	if (!strcmp(command, "count"))
	{
		RECT first = {};
		SendMessageA(Tree, LB_GETITEMRECT, 0, (LPARAM)&first);
		printf("PREFCLICK: %d rows, top %ld, selected %ld, row height %ld\n", Rows(),
			(long)SendMessageA(Tree, LB_GETTOPINDEX, 0, 0), (long)SendMessageA(Tree, LB_GETCURSEL, 0, 0),
			first.bottom - first.top);
		return 0;
	}

	if (!strcmp(command, "settop") && argc >= 3)
	{
		SendMessageA(Tree, LB_SETTOPINDEX, (WPARAM)atoi(argv[2]), 0);
		return 0;
	}

	if (!strcmp(command, "walk") && argc >= 3)
	{
		int took = Walk(atoi(argv[2]), argc >= 4 ? atoi(argv[3]) : 0);
		printf("PREFCLICK: walked %d rows; %d rows now\n", took, Rows());
		SendMessageA(Tree, LB_SETTOPINDEX, 0, 0);
		return 0;
	}

	if (argc < 4)
		return Usage();
	int row = atoi(argv[2]), depth = atoi(argv[3]);

	if (!strcmp(command, "toggle"))
	{
		int before = Rows();
		ClickRow(row, BoxX(depth), 0);
		printf("PREFCLICK: rows %d -> %d\n", before, Rows());
		SendMessageA(Tree, LB_SETTOPINDEX, 0, 0);
		return 0;
	}

	if (!strcmp(command, "select"))
	{
		ClickRow(row, BoxX(depth), 0);
		ListVisible("    editor ");
		return 0;
	}

	if (!strcmp(command, "type") && argc >= 5)
	{
		// As a player types: the text all selected, the characters, Enter.
		ClickRow(row, BoxX(depth), 0);
		HWND edit = Visible("Edit", NULL);
		if (!edit)
		{
			printf("PREFCLICK: no edit box\n");
			return 1;
		}
		Describe(edit, "PREFCLICK: before: ");
		SendMessageA(edit, WM_SETFOCUS, 0, 0);
		SendMessageA(edit, EM_SETSEL, 0, -1);
		for (const char* c = argv[4]; *c; c++)
			SendMessageA(edit, WM_CHAR, (WPARAM)(unsigned char)*c, 1);
		Describe(edit, "PREFCLICK: typed: ");
		if (argc < 6 || strcmp(argv[5], "noenter"))
		{
			SendMessageA(edit, WM_KEYDOWN, VK_RETURN, 1);
			SendMessageA(edit, WM_CHAR, '\r', 1);
			SendMessageA(edit, WM_KEYUP, VK_RETURN, 0xC0000001);
		}
		Sleep(300);
		if (IsWindow(edit))
			Describe(edit, "PREFCLICK: after: ");
		else
			printf("PREFCLICK: after: the edit box is gone\n");
		return 0;
	}

	if (!strcmp(command, "key") && argc >= 5)
	{
		ClickRow(row, BoxX(depth) + 30, 0);
		int before = Rows();
		int key = (int)strtol(argv[4], NULL, 0);
		SendMessageA(Tree, WM_KEYDOWN, (WPARAM)key, 1);
		SendMessageA(Tree, WM_KEYUP, (WPARAM)key, 0xC0000001);
		Sleep(300);
		printf("PREFCLICK: key 0x%02x on row %d: rows %d -> %d, selected %ld\n", key, row, before, Rows(),
			(long)SendMessageA(Tree, LB_GETCURSEL, 0, 0));
		return 0;
	}

	return Usage();
}
