/*
 * Process information and debugging test (test tier 6).
 *
 * Checks the parts of wine's Unix side that read process and CPU state
 * from the host, and the debugger paths that need the server to write
 * another process's memory (PEB BeingDebugged), which on OpenBSD cannot
 * use ptrace():
 *   - GetProcessMemoryInfo: working set and private (pagefile) usage, of
 *     this process (ntdll) and of another one (wineserver)
 *   - GetSystemTimes: idle/kernel/user time that tracks real CPU use
 *   - CreateProcess(DEBUG_ONLY_THIS_PROCESS): child sees IsDebuggerPresent
 *   - DebugActiveProcess on a running process: attach, break-in, child
 *     sees IsDebuggerPresent, DebugActiveProcessStop
 * Output is TAP style; the exit code is the number of failed checks.
 *
 *   x86_64-w64-mingw32-gcc -O2 -Wall -o sysinfo.exe sysinfo.c -lpsapi
 */
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>

static int test_no, failures;

static void check(int cond, const char *name)
{
	printf("%s %d - %s\n", cond ? "ok" : "not ok", ++test_no, name);
	if (!cond)
		failures++;
	fflush(stdout);
}

static ULONGLONG ft(FILETIME f)
{
	return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime;
}

static BOOL spawn(const char *mode, DWORD flags, PROCESS_INFORMATION *pi);

static void test_memory_other(void)
{
	PROCESS_MEMORY_COUNTERS pmc;
	PROCESS_INFORMATION pi;

	if (!spawn("sleep", 0, &pi)) {
		check(0, "CreateProcess(child for memory info)");
		return;
	}
	Sleep(1000);
	pmc.cb = sizeof(pmc);
	check(GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc)),
	    "GetProcessMemoryInfo(other process)");
	printf("# child: working set %llu KB, private %llu KB\n",
	    (ULONGLONG)pmc.WorkingSetSize >> 10,
	    (ULONGLONG)pmc.PagefileUsage >> 10);
	check(pmc.WorkingSetSize > 0 && pmc.PagefileUsage > 0,
	    "other process: working set and private usage > 0");
	TerminateProcess(pi.hProcess, 0);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

static void test_memory(void)
{
	PROCESS_MEMORY_COUNTERS_EX before, after;
	SIZE_T size = 64 << 20;
	char *p;

	before.cb = after.cb = sizeof(before);
	check(GetProcessMemoryInfo(GetCurrentProcess(),
	    (PROCESS_MEMORY_COUNTERS *)&before, sizeof(before)),
	    "GetProcessMemoryInfo");
	check(before.WorkingSetSize > 0, "WorkingSetSize > 0");
	check(before.PagefileUsage > 0, "PagefileUsage > 0");

	p = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	memset(p, 1, size);
	GetProcessMemoryInfo(GetCurrentProcess(),
	    (PROCESS_MEMORY_COUNTERS *)&after, sizeof(after));
	printf("# working set %llu -> %llu KB, private %llu -> %llu KB\n",
	    (ULONGLONG)before.WorkingSetSize >> 10,
	    (ULONGLONG)after.WorkingSetSize >> 10,
	    (ULONGLONG)before.PagefileUsage >> 10,
	    (ULONGLONG)after.PagefileUsage >> 10);
	check(after.WorkingSetSize >= before.WorkingSetSize + size / 2,
	    "working set grows after touching 64 MB");
	check(after.PeakWorkingSetSize >= after.WorkingSetSize,
	    "PeakWorkingSetSize >= WorkingSetSize");
	check(after.PagefileUsage >= before.PagefileUsage + size / 2,
	    "private usage grows after committing 64 MB");
	VirtualFree(p, 0, MEM_RELEASE);
}

static void test_cpu_times(void)
{
	FILETIME i0, k0, u0, i1, k1, u1;
	ULONGLONG start, idle, kernel, user;
	volatile unsigned long spin = 0;

	check(GetSystemTimes(&i0, &k0, &u0), "GetSystemTimes");
	start = GetTickCount64();
	while (GetTickCount64() - start < 1000)
		spin++;
	GetSystemTimes(&i1, &k1, &u1);
	idle = ft(i1) - ft(i0);
	kernel = ft(k1) - ft(k0);
	user = ft(u1) - ft(u0);
	printf("# over 1 s of spinning: idle %llu ms, kernel (incl. idle) "
	    "%llu ms, user %llu ms\n", idle / 10000, kernel / 10000,
	    user / 10000);
	check(user >= 5000000, "user time advances >= 0.5 s while spinning");
	check(kernel >= idle, "kernel time includes idle time");
	check(ft(k1) >= ft(i1), "total kernel time >= total idle time");
}

/* child modes */
static int child_report(void)
{
	return IsDebuggerPresent() ? 1 : 2;
}

static int child_wait_debugger(void)
{
	DWORD i;

	/* wait up to 10 s for a debugger to attach */
	for (i = 0; i < 1000; i++) {
		if (IsDebuggerPresent()) {
			/* give the debugger's break-in thread time to run */
			Sleep(2000);
			return 1;
		}
		Sleep(10);
	}
	return 2;
}

static BOOL spawn(const char *mode, DWORD flags, PROCESS_INFORMATION *pi)
{
	char cmd[MAX_PATH + 32], self[MAX_PATH];
	STARTUPINFOA si;

	GetModuleFileNameA(NULL, self, sizeof(self));
	snprintf(cmd, sizeof(cmd), "\"%s\" %s", self, mode);
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	return CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL,
	    &si, pi);
}

/* Run the debug loop until the process exits; counts events. */
static DWORD debug_loop(DWORD pid, int *created, int *breakpoints)
{
	DEBUG_EVENT ev;
	DWORD code = (DWORD)-1;

	for (;;) {
		if (!WaitForDebugEvent(&ev, 20000)) {
			printf("# WaitForDebugEvent: error %lu\n", GetLastError());
			return (DWORD)-1;
		}
		switch (ev.dwDebugEventCode) {
		case CREATE_PROCESS_DEBUG_EVENT:
			(*created)++;
			CloseHandle(ev.u.CreateProcessInfo.hFile);
			break;
		case LOAD_DLL_DEBUG_EVENT:
			CloseHandle(ev.u.LoadDll.hFile);
			break;
		case EXCEPTION_DEBUG_EVENT:
			if (ev.u.Exception.ExceptionRecord.ExceptionCode ==
			    EXCEPTION_BREAKPOINT)
				(*breakpoints)++;
			break;
		case EXIT_PROCESS_DEBUG_EVENT:
			code = ev.u.ExitProcess.dwExitCode;
			break;
		}
		ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
		if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT &&
		    ev.dwProcessId == pid)
			return code;
	}
}

static void test_debug_create(void)
{
	PROCESS_INFORMATION pi;
	int created = 0, breakpoints = 0;
	DWORD code;

	check(spawn("report", DEBUG_ONLY_THIS_PROCESS, &pi),
	    "CreateProcess(DEBUG_ONLY_THIS_PROCESS)");
	code = debug_loop(pi.dwProcessId, &created, &breakpoints);
	check(created == 1, "CREATE_PROCESS_DEBUG_EVENT received");
	check(breakpoints >= 1, "initial breakpoint received");
	check(code == 1, "debuggee sees IsDebuggerPresent()");
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

static void test_debug_attach(void)
{
	PROCESS_INFORMATION pi;
	int created = 0, breakpoints = 0;
	DWORD code;
	BOOL ok;

	check(spawn("wait", 0, &pi), "CreateProcess(child)");
	Sleep(500);	/* let the child finish initialising */
	ok = DebugActiveProcess(pi.dwProcessId);
	if (!ok)
		printf("# DebugActiveProcess: error %lu\n", GetLastError());
	check(ok, "DebugActiveProcess on a running process");
	if (!ok) {
		TerminateProcess(pi.hProcess, 3);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		return;
	}
	code = debug_loop(pi.dwProcessId, &created, &breakpoints);
	check(created == 1, "attach: CREATE_PROCESS_DEBUG_EVENT received");
	check(breakpoints >= 1, "attach: break-in breakpoint received");
	check(code == 1, "attached process sees IsDebuggerPresent()");
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

static void test_debug_detach(void)
{
	PROCESS_INFORMATION pi;
	DEBUG_EVENT ev;
	DWORD code = 0;
	int i;

	check(spawn("wait", 0, &pi), "CreateProcess(child for detach)");
	Sleep(500);
	if (!DebugActiveProcess(pi.dwProcessId)) {
		check(0, "detach: DebugActiveProcess");
		TerminateProcess(pi.hProcess, 3);
		return;
	}
	/* drain the attach events (create process, threads, dlls, break-in) */
	for (i = 0; i < 200 && WaitForDebugEvent(&ev, 2000); i++) {
		if (ev.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
			CloseHandle(ev.u.CreateProcessInfo.hFile);
		else if (ev.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT)
			CloseHandle(ev.u.LoadDll.hFile);
		ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
		if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
			break;
	}
	check(DebugActiveProcessStop(pi.dwProcessId), "DebugActiveProcessStop");
	check(WaitForSingleObject(pi.hProcess, 20000) == WAIT_OBJECT_0,
	    "detached process exits");
	GetExitCodeProcess(pi.hProcess, &code);
	/* 1 if the child polled IsDebuggerPresent() while attached, 2 if the
	 * detach came first; anything else means it was killed or crashed */
	check(code == 1 || code == 2, "detached process keeps running");
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

int main(int argc, char **argv)
{
	if (argc > 1 && !strcmp(argv[1], "report"))
		return child_report();
	if (argc > 1 && !strcmp(argv[1], "wait"))
		return child_wait_debugger();
	if (argc > 1 && !strcmp(argv[1], "sleep")) {
		Sleep(5000);
		return 0;
	}

	test_memory();
	test_memory_other();
	test_cpu_times();
	test_debug_create();
	test_debug_attach();
	test_debug_detach();
	printf("1..%d\n", test_no);
	return failures;
}
