/*
 * Debugging aid: LD_PRELOAD shim that reports overlapping memcpy() calls.
 *
 * OpenBSD's libc memcpy() aborts on overlapping buffers after logging
 * "backwards memcpy" to syslog, with no hint of the caller.  This shim
 * prints a backtrace (module+offset) of each overlapping call to stderr and then performs
 * the copy with memmove(), so a run continues and every offender is listed.
 * Never use it to work around such a bug; fix the caller.
 *
 *   cc -shared -fPIC -O1 -o memcpy-overlap.so memcpy-overlap.c -lexecinfo
 *   LD_PRELOAD=$PWD/memcpy-overlap.so wine program.exe
 */
#include <dlfcn.h>
#include <execinfo.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void *
memcpy(void *dst, const void *src, size_t len)
{
	uintptr_t d = (uintptr_t)dst, s = (uintptr_t)src;

	if (len && ((d < s && d + len > s) || (s < d && s + len > d))) {
		static const char msg[] = "memcpy-overlap: overlapping memcpy\n";
		void *frames[32];
		char line[512];
		Dl_info info;
		int i, n;

		write(2, msg, sizeof(msg) - 1);
		n = backtrace(frames, 32);
		/* module + offset, for llvm-addr2line -e module offset */
		for (i = 1; i < n; i++) {
			if (!dladdr(frames[i], &info) || !info.dli_fname)
				continue;
			snprintf(line, sizeof(line), "  %s+0x%lx\n",
			    info.dli_fname, (unsigned long)((uintptr_t)frames[i] -
			    (uintptr_t)info.dli_fbase));
			write(2, line, strlen(line));
		}
	}
	return memmove(dst, src, len);
}
