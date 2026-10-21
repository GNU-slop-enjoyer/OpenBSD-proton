/*
 * Debugging aid: LD_PRELOAD shim that reports re-entrant malloc calls.
 *
 * OpenBSD's malloc aborts with "malloc(): recursive call" when malloc is
 * entered again on a thread that is already inside it, which happens when a
 * signal handler allocates.  This shim counts the nesting per thread and,
 * on re-entry, writes a module+offset backtrace of the inner call to stderr
 * before passing it on (libc then aborts as usual).
 *
 *   cc -shared -fPIC -O1 -o malloc-reentry.so malloc-reentry.c -lexecinfo
 *   LD_PRELOAD=$PWD/malloc-reentry.so wine program.exe
 */
#include <dlfcn.h>
#include <execinfo.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void *(*real_malloc)(size_t);
static void *(*real_calloc)(size_t, size_t);
static void *(*real_realloc)(void *, size_t);
static void (*real_free)(void *);
static __thread int depth;

static void __attribute__((constructor)) init(void)
{
	real_malloc = dlsym(RTLD_NEXT, "malloc");
	real_calloc = dlsym(RTLD_NEXT, "calloc");
	real_realloc = dlsym(RTLD_NEXT, "realloc");
	real_free = dlsym(RTLD_NEXT, "free");
}

static void report(const char *what)
{
	void *frames[48];
	char line[512];
	Dl_info info;
	int i, n, len;

	len = snprintf(line, sizeof(line), "malloc-reentry: nested %s\n", what);
	write(2, line, len);
	n = backtrace(frames, 48);
	for (i = 1; i < n; i++) {
		if (!dladdr(frames[i], &info) || !info.dli_fname)
			continue;
		len = snprintf(line, sizeof(line), "  %s+0x%lx\n", info.dli_fname,
		    (unsigned long)((uintptr_t)frames[i] - (uintptr_t)info.dli_fbase));
		write(2, line, len);
	}
}

#define ENTER(what) do { if (depth++) report(what); } while (0)
#define LEAVE() depth--

void *malloc(size_t n)
{
	void *p;
	ENTER("malloc");
	p = real_malloc(n);
	LEAVE();
	return p;
}

void *calloc(size_t a, size_t b)
{
	void *p;
	ENTER("calloc");
	p = real_calloc(a, b);
	LEAVE();
	return p;
}

void *realloc(void *o, size_t n)
{
	void *p;
	ENTER("realloc");
	p = real_realloc(o, n);
	LEAVE();
	return p;
}

void free(void *p)
{
	ENTER("free");
	real_free(p);
	LEAVE();
}
