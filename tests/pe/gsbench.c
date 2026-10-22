/*
 * Cost of TEB access through %gs (measurement).
 *
 * Windows x86_64 code reads the TEB with %gs:-relative instructions
 * (NtCurrentTeb(), GetLastError(), TlsGetValue(), compiler intrinsics in
 * game code).  OpenBSD gives user processes no way to set the %gs base, so
 * wine catches the fault on each such instruction and rewrites it
 * (dlls/ntdll/unix/gs_openbsd.c); after that it should run at full speed.
 * This times those accesses, and the first execution of a fresh %gs
 * instruction (the fault and rewrite), and prints ns per operation.
 *
 *   x86_64-w64-mingw32-gcc -O2 -Wall -o gsbench.exe gsbench.c
 */
#include <windows.h>
#include <stdio.h>

#define N 10000000

static double now(void)
{
	static LARGE_INTEGER freq;
	LARGE_INTEGER t;

	if (!freq.QuadPart)
		QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&t);
	return (double)t.QuadPart / freq.QuadPart;
}

static __attribute__((noinline)) void *read_gs_teb(void)
{
	void *teb;

	__asm__ volatile("movq %%gs:0x30, %0" : "=r"(teb));
	return teb;
}

/* a short (4-byte) form, mov %gs:(%rcx),%rax, which cannot be patched in
 * place with a 5-byte jump */
static __attribute__((noinline)) void *read_gs_short(void)
{
	void *teb;

	__asm__ volatile("movq $0x30, %%rcx\n\t"
	                 ".byte 0x65, 0x48, 0x8b, 0x01\n\t"   /* mov %gs:(%rcx),%rax */
	                 "movq %%rax, %0" : "=r"(teb) :: "rax", "rcx");
	return teb;
}

/* the same instruction in a function followed by int3 padding up to the
 * next 32-byte boundary, as compilers pad between functions: the padding
 * can hold the jump that the instruction has no room for */
void *gs_short_padded(void);
__asm__( ".text\n\t"
         ".p2align 5\n"
         "gs_short_padded:\n\t"
         "movq $0x30, %rcx\n\t"
         ".byte 0x65, 0x48, 0x8b, 0x01\n\t"   /* mov %gs:(%rcx),%rax */
         "ret\n\t"
         ".p2align 5, 0xcc\n\t" );

static void report(const char *what, double s, long n)
{
	printf("%-40s %8.2f ns/op\n", what, s * 1e9 / n);
}

int main(void)
{
	DWORD tls = TlsAlloc();
	volatile void *sink;
	volatile DWORD dsink;
	unsigned char *code;
	double t;
	long i;

	t = now();
	for (i = 0; i < N; i++)
		sink = read_gs_teb();
	report("mov %gs:0x30 (inline, after rewrite)", now() - t, N);

	t = now();
	for (i = 0; i < N / 100; i++)
		sink = read_gs_short();
	report("mov %gs:(%rcx) (4-byte form)", now() - t, N / 100);

	t = now();
	for (i = 0; i < N; i++)
		sink = gs_short_padded();
	report("mov %gs:(%rcx), int3 padding nearby", now() - t, N);

	t = now();
	for (i = 0; i < N; i++)
		dsink = GetLastError();
	report("GetLastError()", now() - t, N);

	TlsSetValue(tls, (void *)1);
	t = now();
	for (i = 0; i < N; i++)
		sink = TlsGetValue(tls);
	report("TlsGetValue()", now() - t, N);

	/* first execution of fresh %gs instructions: copies of a function
	 * "mov %gs:0x30,%rax; ret" in new executable pages */
	code = VirtualAlloc(NULL, 4096 * 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (code) {
		static const unsigned char fn[] = {
			0x65, 0x48, 0x8b, 0x04, 0x25, 0x30, 0x00, 0x00, 0x00, /* mov %gs:0x30,%rax */
			0xc3                                                  /* ret */
		};
		int k;

		for (k = 0; k < 64; k++)
			memcpy(code + k * 4096, fn, sizeof(fn));
		FlushInstructionCache(GetCurrentProcess(), code, 4096 * 64);
		t = now();
		for (k = 0; k < 64; k++)
			sink = ((void *(*)(void))(code + k * 4096))();
		report("first run of a new %gs instruction", now() - t, 64);
	}
	(void)sink;
	(void)dsink;
	return 0;
}
