/*
 * Minimal freestanding x86_64 Windows GUI test (test tier 7).
 *
 * Creates a top-level window, paints a known colour in WM_PAINT, reads it
 * back through the window DC, and drives the message loop with a timer.
 * Output is TAP on stdout; the exit code is the number of failed checks.
 * Needs a working display driver (winex11.drv with an X server such as
 * Xvfb).
 */

typedef void *HANDLE, *HWND, *HDC, *HBRUSH, *HINSTANCE, *HICON, *HCURSOR,
	     *HMENU;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef int BOOL;
typedef unsigned short WORD, ATOM;
typedef long long LONG_PTR;
typedef unsigned long long UINT_PTR, WPARAM;
typedef long long LPARAM, LRESULT;
typedef long LONG;
typedef DWORD COLORREF;

#define WINAPI __attribute__((ms_abi))
typedef LRESULT (WINAPI *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x, y; } POINT;
typedef struct {
	HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time;
	POINT pt; DWORD lPrivate;
} MSG;
typedef struct {
	HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate;
	unsigned char rgbReserved[32];
} PAINTSTRUCT;
typedef struct {
	UINT style; WNDPROC lpfnWndProc; int cbClsExtra; int cbWndExtra;
	HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor; HBRUSH hbrBackground;
	const char *lpszMenuName; const char *lpszClassName;
} WNDCLASSA;

#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define WS_OVERLAPPEDWINDOW 0x00cf0000
#define WS_VISIBLE 0x10000000
#define WM_DESTROY 0x0002
#define WM_PAINT 0x000f
#define WM_TIMER 0x0113
#define SW_SHOW 5
#define RGB(r, g, b) ((COLORREF)((r) | ((g) << 8) | ((b) << 16)))

WINAPI HANDLE GetStdHandle(DWORD);
WINAPI BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
WINAPI void ExitProcess(UINT);
WINAPI HINSTANCE GetModuleHandleA(const char *);
WINAPI ATOM RegisterClassA(const WNDCLASSA *);
WINAPI HWND CreateWindowExA(DWORD, const char *, const char *, DWORD, int, int,
			    int, int, HWND, HMENU, HINSTANCE, void *);
WINAPI BOOL ShowWindow(HWND, int);
WINAPI BOOL UpdateWindow(HWND);
WINAPI LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
WINAPI BOOL GetMessageA(MSG *, HWND, UINT, UINT);
WINAPI BOOL TranslateMessage(const MSG *);
WINAPI LRESULT DispatchMessageA(const MSG *);
WINAPI void PostQuitMessage(int);
WINAPI BOOL DestroyWindow(HWND);
WINAPI UINT_PTR SetTimer(HWND, UINT_PTR, UINT, void *);
WINAPI HDC BeginPaint(HWND, PAINTSTRUCT *);
WINAPI BOOL EndPaint(HWND, const PAINTSTRUCT *);
WINAPI HDC GetDC(HWND);
WINAPI int ReleaseDC(HWND, HDC);
WINAPI BOOL GetClientRect(HWND, RECT *);
WINAPI int FillRect(HDC, const RECT *, HBRUSH);
WINAPI HBRUSH CreateSolidBrush(COLORREF);
WINAPI BOOL DeleteObject(void *);
WINAPI COLORREF GetPixel(HDC, int, int);

static const COLORREF colour = RGB(0x20, 0x80, 0xe0);
static HANDLE out;
static int failures, testno, painted, ticks;

static unsigned int slen(const char *s)
{
	unsigned int n = 0;

	while (s[n])
		n++;
	return n;
}

static void put(const char *s)
{
	DWORD w;

	WriteFile(out, s, slen(s), &w, 0);
}

static void puthex(unsigned long v)
{
	char buf[9];
	int i;

	for (i = 7; i >= 0; i--, v >>= 4)
		buf[i] = "0123456789abcdef"[v & 15];
	buf[8] = 0;
	put(buf);
}

static void putnum(unsigned int v)
{
	char buf[11], *p = buf + sizeof(buf) - 1;

	*p = 0;
	do {
		*--p = '0' + v % 10;
		v /= 10;
	} while (v);
	put(p);
}

static void check(int ok, const char *name)
{
	testno++;
	if (!ok) {
		failures++;
		put("not ");
	}
	put("ok ");
	putnum(testno);
	put(" - ");
	put(name);
	put("\r\n");
}

static WINAPI LRESULT wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	PAINTSTRUCT ps;
	RECT rc;
	HBRUSH brush;
	HDC dc;

	switch (msg) {
	case WM_PAINT:
		dc = BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rc);
		brush = CreateSolidBrush(colour);
		FillRect(dc, &rc, brush);
		DeleteObject(brush);
		EndPaint(hwnd, &ps);
		painted++;
		return 0;
	case WM_TIMER:
		/* give the window a few timer ticks to be mapped and painted */
		if (++ticks >= 5)
			DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(hwnd, msg, wp, lp);
}

void WINAPI mainCRTStartup(void)
{
	WNDCLASSA wc = { 0 };
	HWND hwnd;
	MSG msg;
	HDC dc;
	COLORREF px = 0;

	out = GetStdHandle(STD_OUTPUT_HANDLE);
	put("1..5\r\n");

	wc.lpfnWndProc = wndproc;
	wc.hInstance = GetModuleHandleA(0);
	wc.lpszClassName = "obsdproton";
	check(RegisterClassA(&wc) != 0, "RegisterClass");

	hwnd = CreateWindowExA(0, "obsdproton", "OpenBSD Proton GUI test",
			       WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 200,
			       150, 0, 0, wc.hInstance, 0);
	check(hwnd != 0, "CreateWindowEx");
	if (!hwnd)
		ExitProcess(failures + 3);
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	check(painted > 0, "WM_PAINT delivered by UpdateWindow");

	dc = GetDC(hwnd);
	px = GetPixel(dc, 50, 50);
	ReleaseDC(hwnd, dc);
	check(px == colour, "painted pixel reads back");
	if (px != colour) {
		put("# got ");
		puthex(px);
		put(" expected ");
		puthex(colour);
		put("\r\n");
	}

	SetTimer(hwnd, 1, 100, 0);
	while (GetMessageA(&msg, 0, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
	check(ticks >= 5, "message loop with WM_TIMER until WM_QUIT");

	ExitProcess(failures);
}
