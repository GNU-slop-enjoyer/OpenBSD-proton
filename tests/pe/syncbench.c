/*
 * Synchronization latency benchmark.
 *
 * Wine implements waits on kernel objects (events, semaphores, mutexes)
 * either in wineserver (one request per wait/set: the only option on
 * OpenBSD) or in-process (Proton's fsync/esync on Linux, ntsync).  Games
 * that synchronize threads every frame feel the difference.  This measures
 *   - event ping-pong between two threads (SetEvent + wait, both ways)
 *   - an uncontended wait on a signaled event
 *   - ReleaseSemaphore + wait on a semaphore
 *   - mutex acquire/release
 *   - critical section enter/leave (never leaves the process; baseline)
 *   - SRW lock + condition variable ping-pong and WaitOnAddress ping-pong
 *     between two threads: these block through NtWaitForAlertByThreadId,
 *     which wine implements with futexes (Linux, macOS), kqueue
 *     EVFILT_USER (BSDs) or a wineserver event
 * and prints microseconds per operation.  No pass/fail.
 *
 *   x86_64-w64-mingw32-gcc -O2 -Wall -o syncbench.exe syncbench.c -lsynchronization
 */
#include <windows.h>
#include <stdio.h>

#define ROUNDS 20000

static HANDLE ping, pong;
static SRWLOCK lock = SRWLOCK_INIT;
static CONDITION_VARIABLE cv = CONDITION_VARIABLE_INIT;
static volatile LONG turn, addr_turn;

static double now(void)
{
	static LARGE_INTEGER freq;
	LARGE_INTEGER t;

	if (!freq.QuadPart)
		QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&t);
	return (double)t.QuadPart / freq.QuadPart;
}

static DWORD WINAPI ponger(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < ROUNDS; i++) {
		WaitForSingleObject(ping, INFINITE);
		SetEvent(pong);
	}
	return 0;
}

static DWORD WINAPI cv_ponger(void *arg)
{
	int i;

	(void)arg;
	AcquireSRWLockExclusive(&lock);
	for (i = 0; i < ROUNDS; i++) {
		while (turn != 1)
			SleepConditionVariableSRW(&cv, &lock, INFINITE, 0);
		turn = 0;
		WakeConditionVariable(&cv);
	}
	ReleaseSRWLockExclusive(&lock);
	return 0;
}

static DWORD WINAPI addr_ponger(void *arg)
{
	LONG zero = 0;
	int i;

	(void)arg;
	for (i = 0; i < ROUNDS; i++) {
		/* WaitOnAddress blocks while the value equals the compare value */
		while (addr_turn != 1)
			WaitOnAddress(&addr_turn, &zero, sizeof(zero), INFINITE);
		addr_turn = 0;
		WakeByAddressSingle((void *)&addr_turn);
	}
	return 0;
}

static void report(const char *what, double seconds, int ops)
{
	printf("%-36s %8.2f us/op  (%d ops, %.3f s)\n", what,
	    seconds * 1e6 / ops, ops, seconds);
	fflush(stdout);
}

int main(void)
{
	CRITICAL_SECTION cs;
	HANDLE thread, ev, sem, mtx;
	double t;
	int i;

	ping = CreateEventW(NULL, FALSE, FALSE, NULL);
	pong = CreateEventW(NULL, FALSE, FALSE, NULL);
	thread = CreateThread(NULL, 0, ponger, NULL, 0, NULL);
	t = now();
	for (i = 0; i < ROUNDS; i++) {
		SetEvent(ping);
		WaitForSingleObject(pong, INFINITE);
	}
	report("event ping-pong (round trip)", now() - t, ROUNDS);
	WaitForSingleObject(thread, INFINITE);

	ev = CreateEventW(NULL, TRUE, TRUE, NULL);
	t = now();
	for (i = 0; i < ROUNDS * 5; i++)
		WaitForSingleObject(ev, 0);
	report("wait on signaled event", now() - t, ROUNDS * 5);

	sem = CreateSemaphoreW(NULL, 0, 1, NULL);
	t = now();
	for (i = 0; i < ROUNDS * 5; i++) {
		ReleaseSemaphore(sem, 1, NULL);
		WaitForSingleObject(sem, 0);
	}
	report("semaphore release + wait", now() - t, ROUNDS * 5);

	mtx = CreateMutexW(NULL, FALSE, NULL);
	t = now();
	for (i = 0; i < ROUNDS * 5; i++) {
		WaitForSingleObject(mtx, 0);
		ReleaseMutex(mtx);
	}
	report("mutex acquire + release", now() - t, ROUNDS * 5);

	thread = CreateThread(NULL, 0, cv_ponger, NULL, 0, NULL);
	t = now();
	AcquireSRWLockExclusive(&lock);
	for (i = 0; i < ROUNDS; i++) {
		turn = 1;
		WakeConditionVariable(&cv);
		while (turn != 0)
			SleepConditionVariableSRW(&cv, &lock, INFINITE, 0);
	}
	ReleaseSRWLockExclusive(&lock);
	report("condition variable ping-pong", now() - t, ROUNDS);
	WaitForSingleObject(thread, INFINITE);

	thread = CreateThread(NULL, 0, addr_ponger, NULL, 0, NULL);
	t = now();
	for (i = 0; i < ROUNDS; i++) {
		LONG one = 1;

		addr_turn = 1;
		WakeByAddressSingle((void *)&addr_turn);
		while (addr_turn != 0)
			WaitOnAddress(&addr_turn, &one, sizeof(one), INFINITE);
	}
	report("WaitOnAddress ping-pong", now() - t, ROUNDS);
	WaitForSingleObject(thread, INFINITE);

	InitializeCriticalSection(&cs);
	t = now();
	for (i = 0; i < ROUNDS * 50; i++) {
		EnterCriticalSection(&cs);
		LeaveCriticalSection(&cs);
	}
	report("critical section enter + leave", now() - t, ROUNDS * 50);
	return 0;
}
