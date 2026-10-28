/*
 * wakeups.exe: looks for lost wake-ups in the primitives games build
 * their job queues and thread pools on: condition variables (SRW lock),
 * WaitOnAddress/WakeByAddress*, and auto-reset events.
 *
 * Each test runs producer/consumer rounds between several threads.  A
 * waiter that sleeps through a wake-up stays asleep until its timeout
 * (1 s) fires; the test counts those timeouts while the condition was
 * already true, which a correct implementation never produces.  Output
 * is TAP; the exit status is the number of failed tests.
 *
 * Copyright (c) 2026 The OpenBSD Proton project contributors
 * Distributed under the LGPL 2.1 or later.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#define THREADS	8
#define ROUNDS	20000

static SRWLOCK lock = SRWLOCK_INIT;
static CONDITION_VARIABLE cv = CONDITION_VARIABLE_INIT;
static volatile LONG queue, done, lost;
static int ntest, nfail;

static void
ok(int cond, const char *what)
{
	ntest++;
	if (!cond)
		nfail++;
	printf("%s %d - %s\n", cond ? "ok" : "not ok", ntest, what);
	fflush(stdout);
}

/* ---- condition variable ---- */

static DWORD WINAPI
cv_consumer(void *arg)
{
	(void)arg;
	AcquireSRWLockExclusive(&lock);
	for (;;) {
		while (queue == 0 && !done)
			if (!SleepConditionVariableSRW(&cv, &lock, 1000, 0) &&
			    queue != 0)
				InterlockedIncrement(&lost);
		if (queue == 0 && done)
			break;
		queue--;
	}
	ReleaseSRWLockExclusive(&lock);
	return 0;
}

static void
test_cv(void)
{
	HANDLE t[THREADS];
	int i;

	queue = done = lost = 0;
	for (i = 0; i < THREADS; i++)
		t[i] = CreateThread(NULL, 0, cv_consumer, NULL, 0, NULL);
	for (i = 0; i < ROUNDS; i++) {
		AcquireSRWLockExclusive(&lock);
		queue++;
		ReleaseSRWLockExclusive(&lock);
		WakeConditionVariable(&cv);
		if (i % 64 == 0)
			Sleep(0);
	}
	AcquireSRWLockExclusive(&lock);
	done = 1;
	ReleaseSRWLockExclusive(&lock);
	WakeAllConditionVariable(&cv);
	WaitForMultipleObjects(THREADS, t, TRUE, INFINITE);
	printf("# condition variable: %ld lost wake-ups\n", (long)lost);
	ok(lost == 0 && queue == 0, "SleepConditionVariableSRW/WakeConditionVariable");
}

/* ---- WaitOnAddress ---- */

static volatile LONG word;
static volatile LONG consumed;

static DWORD WINAPI
woa_consumer(void *arg)
{
	LONG zero = 0;

	(void)arg;
	for (;;) {
		LONG v = word;
		if (v == 0) {
			if (done)
				break;
			if (!WaitOnAddress((volatile void *)&word, &zero,
			    sizeof(word), 1000) && word != 0)
				InterlockedIncrement(&lost);
			continue;
		}
		if (InterlockedCompareExchange(&word, v - 1, v) == v)
			InterlockedIncrement(&consumed);
	}
	return 0;
}

static void
test_woa(void)
{
	HANDLE t[THREADS];
	int i;

	word = done = lost = consumed = 0;
	for (i = 0; i < THREADS; i++)
		t[i] = CreateThread(NULL, 0, woa_consumer, NULL, 0, NULL);
	for (i = 0; i < ROUNDS; i++) {
		InterlockedIncrement(&word);
		WakeByAddressSingle((void *)&word);
		if (i % 64 == 0)
			Sleep(0);
	}
	while (word != 0)
		Sleep(1);
	done = 1;
	WakeByAddressAll((void *)&word);
	WaitForMultipleObjects(THREADS, t, TRUE, INFINITE);
	printf("# WaitOnAddress: %ld lost wake-ups, %ld consumed\n",
	    (long)lost, (long)consumed);
	ok(lost == 0 && consumed == ROUNDS, "WaitOnAddress/WakeByAddressSingle");
}

/* ---- auto-reset event ping-pong ---- */

static HANDLE ping, pong;

static DWORD WINAPI
ponger(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < ROUNDS; i++) {
		if (WaitForSingleObject(ping, 1000) == WAIT_TIMEOUT)
			InterlockedIncrement(&lost);
		SetEvent(pong);
	}
	return 0;
}

static void
test_events(void)
{
	HANDLE t;
	int i;

	lost = 0;
	ping = CreateEventW(NULL, FALSE, FALSE, NULL);
	pong = CreateEventW(NULL, FALSE, FALSE, NULL);
	t = CreateThread(NULL, 0, ponger, NULL, 0, NULL);
	for (i = 0; i < ROUNDS; i++) {
		SetEvent(ping);
		if (WaitForSingleObject(pong, 1000) == WAIT_TIMEOUT)
			InterlockedIncrement(&lost);
	}
	WaitForSingleObject(t, INFINITE);
	printf("# events: %ld lost wake-ups\n", (long)lost);
	ok(lost == 0, "auto-reset event ping-pong");
}

int
main(void)
{
	DWORD t0;

	printf("1..3\n");
	t0 = GetTickCount();
	test_cv();
	printf("# %lu ms\n", (unsigned long)(GetTickCount() - t0));
	t0 = GetTickCount();
	test_woa();
	printf("# %lu ms\n", (unsigned long)(GetTickCount() - t0));
	t0 = GetTickCount();
	test_events();
	printf("# %lu ms\n", (unsigned long)(GetTickCount() - t0));
	return nfail;
}
