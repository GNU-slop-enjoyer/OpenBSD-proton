/*
 * Sleep granularity (test tier 6, measurement).
 *
 * Prints how long Sleep(n) and a 1 ms timed wait really take, before and
 * after timeBeginPeriod(1).  OpenBSD's clock ticks at 100 Hz, so without
 * help Sleep(1) takes 20 ms there; proton-wine sleeps precisely once the
 * process asked for a fine timer resolution or WINE_PRECISE_SLEEP=1 is set.
 *
 *   x86_64-w64-mingw32-gcc -O2 -Wall -o sleeptest.exe sleeptest.c -lwinmm
 */
#include <windows.h>
#include <stdio.h>
static double now(void) { LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c); return (double)c.QuadPart / f.QuadPart; }
int main(void)
{
	HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);
	double t, mn, mx;
	int i, n = 200;
	int ms[] = { 0, 1, 2, 5, 16 };
	unsigned k;
	for (k = 0; k < sizeof(ms) / sizeof(ms[0]); k++) {
		mn = 1e9; mx = 0;
		t = now();
		for (i = 0; i < n; i++) {
			double s = now(), d;
			Sleep(ms[k]);
			d = now() - s;
			if (d < mn) mn = d;
			if (d > mx) mx = d;
		}
		printf("Sleep(%2d): mean %6.2f ms  min %6.2f  max %6.2f\n", ms[k], (now() - t) * 1000 / n, mn * 1000, mx * 1000);
	}
	t = now();
	for (i = 0; i < n; i++) WaitForSingleObject(ev, 1);
	printf("WaitForSingleObject(ev, 1): mean %6.2f ms\n", (now() - t) * 1000 / n);
	timeBeginPeriod(1);
	t = now();
	for (i = 0; i < n; i++) Sleep(1);
	printf("Sleep(1) after timeBeginPeriod(1): mean %6.2f ms\n", (now() - t) * 1000 / n);
	return 0;
}
