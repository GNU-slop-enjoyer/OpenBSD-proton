// Smoke test for the toolchain: libc++ containers, std::thread on Win32
// threads, exceptions through libunwind (SEH), and a Win32 API call.
#include <cstdio>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

int main() {
	std::vector<std::string> v{"a", "b", "c"};
	std::mutex m;
	int n = 0;
	std::thread t([&] { std::lock_guard<std::mutex> g(m); n += 42; });
	t.join();
	try {
		throw std::runtime_error("exc");
	} catch (const std::exception &e) {
		std::printf("caught %s\n", e.what());
	}
	auto p = std::make_unique<int>(7);
	std::printf("n=%d v=%zu p=%d tid=%lu\n", n, v.size(), *p,
	    GetCurrentThreadId());
	return n == 42 && *p == 7 ? 0 : 1;
}
