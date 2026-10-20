/*
 * Minimal freestanding x86_64 Windows console test (test tier 6).
 *
 * Built without a C runtime so that only kernel32 is involved.  Each check
 * prints "ok N - name" or "not ok N - name" (TAP style); the process exit
 * code is the number of failed checks.  The checks target code paths that
 * the OpenBSD wine port reimplements:
 *   - %gs-relative TEB access (dlls/ntdll/unix/gs_openbsd.c fault rewriting)
 *   - thread creation and synchronization
 *   - VirtualAlloc/VirtualProtect, including executable memory (W^X)
 *   - file creation, write, read and delete in the temp directory
 */

typedef void *HANDLE;
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned long long SIZE_T;
typedef DWORD (__attribute__((ms_abi)) *THREADPROC)(void *);

#define WINAPI __attribute__((ms_abi))
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define INFINITE 0xffffffff
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE_READ 0x20
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x80
#define INVALID_HANDLE_VALUE ((HANDLE)-1)

WINAPI HANDLE GetStdHandle(DWORD);
WINAPI BOOL WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
WINAPI BOOL ReadFile(HANDLE, void *, DWORD, DWORD *, void *);
WINAPI void ExitProcess(unsigned int);
WINAPI HANDLE CreateThread(void *, SIZE_T, THREADPROC, void *, DWORD, DWORD *);
WINAPI DWORD WaitForSingleObject(HANDLE, DWORD);
WINAPI BOOL GetExitCodeThread(HANDLE, DWORD *);
WINAPI BOOL CloseHandle(HANDLE);
WINAPI DWORD GetCurrentThreadId(void);
WINAPI void *VirtualAlloc(void *, SIZE_T, DWORD, DWORD);
WINAPI BOOL VirtualProtect(void *, SIZE_T, DWORD, DWORD *);
WINAPI BOOL VirtualFree(void *, SIZE_T, DWORD);
WINAPI DWORD GetTempPathA(DWORD, char *);
WINAPI HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD,
			  HANDLE);
WINAPI BOOL DeleteFileA(const char *);

static HANDLE out;
static int failures, testno;

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

static void putnum(unsigned long long v)
{
	char buf[21], *p = buf + sizeof(buf) - 1;

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

static void *teb_via_gs(void)
{
	void *teb;

	/* NtCurrentTeb(): TEB->NtTib.Self lives at %gs:0x30 on x86_64. */
	__asm__ volatile("movq %%gs:0x30, %0" : "=r"(teb));
	return teb;
}

static DWORD teb_tid_via_gs(void)
{
	unsigned long long tid;

	/* TEB->ClientId.UniqueThread at %gs:0x48. */
	__asm__ volatile("movq %%gs:0x48, %0" : "=r"(tid));
	return (DWORD)tid;
}

static WINAPI DWORD worker(void *arg)
{
	volatile unsigned long long sum = 0;
	unsigned long long i, n = (unsigned long long)arg;

	for (i = 1; i <= n; i++)
		sum += i;
	/* The worker's TEB must differ from the main thread's. */
	return teb_tid_via_gs() == GetCurrentThreadId() ? (DWORD)(sum & 0xffff)
							  : 0xdead;
}

void WINAPI mainCRTStartup(void)
{
	HANDLE th, fh;
	DWORD code = 0, old, n;
	unsigned char *mem;
	char path[300];
	const char msg[] = "openbsd-proton file test";
	char buf[sizeof(msg)];
	unsigned int i, plen;

	out = GetStdHandle(STD_OUTPUT_HANDLE);
	put("1..6\r\n");

	void *teb = teb_via_gs();
	check(teb && *(void **)((char *)teb + 0x30) == teb,
	      "TEB self pointer via %gs:0x30");
	check(teb_tid_via_gs() == GetCurrentThreadId(),
	      "TEB thread id via %gs:0x48 matches GetCurrentThreadId");

	th = CreateThread(0, 0, worker, (void *)1000, 0, 0);
	check(th && WaitForSingleObject(th, INFINITE) == 0 &&
	      GetExitCodeThread(th, &code) && code == (500500 & 0xffff),
	      "CreateThread/WaitForSingleObject with per-thread TEB");
	if (th)
		CloseHandle(th);

	mem = VirtualAlloc(0, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (mem) {
		/* mov eax, 42; ret */
		static const unsigned char code42[] = {0xb8, 42, 0, 0, 0, 0xc3};

		for (i = 0; i < sizeof(code42); i++)
			mem[i] = code42[i];
	}
	check(mem != 0, "VirtualAlloc read/write");
	check(mem && VirtualProtect(mem, 4096, PAGE_EXECUTE_READ, &old) &&
	      ((int (*)(void))mem)() == 42,
	      "VirtualProtect to executable and call generated code");
	if (mem)
		VirtualFree(mem, 0, MEM_RELEASE);

	plen = GetTempPathA(sizeof(path) - 32, path);
	for (i = 0; "obsdproton.tmp"[i]; i++)
		path[plen + i] = "obsdproton.tmp"[i];
	path[plen + i] = 0;
	fh = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
			 FILE_ATTRIBUTE_NORMAL, 0);
	int fok = fh != INVALID_HANDLE_VALUE &&
		  WriteFile(fh, msg, sizeof(msg), &n, 0) && n == sizeof(msg);
	if (fh != INVALID_HANDLE_VALUE)
		CloseHandle(fh);
	fh = CreateFileA(path, GENERIC_READ, 0, 0, OPEN_EXISTING,
			 FILE_ATTRIBUTE_NORMAL, 0);
	fok = fok && fh != INVALID_HANDLE_VALUE &&
	      ReadFile(fh, buf, sizeof(buf), &n, 0) && n == sizeof(msg);
	for (i = 0; fok && i < sizeof(msg); i++)
		fok = buf[i] == msg[i];
	if (fh != INVALID_HANDLE_VALUE)
		CloseHandle(fh);
	check(fok && DeleteFileA(path), "temp file write/read/delete");

	ExitProcess(failures);
}
