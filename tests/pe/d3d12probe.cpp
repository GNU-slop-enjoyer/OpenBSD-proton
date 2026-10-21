/*
 * D3D12 probe: try to create a D3D12 device on the default adapter at
 * feature level 11_0 and print the result and which d3d12 DLL was loaded.
 * Used to check vkd3d-proton (or wine's builtin d3d12) on a given machine.
 * Needs a mingw-w64 C++ toolchain.
 *
 * Exit status: 0 if a device was created, 1 otherwise.
 */
#include <windows.h>
#include <d3d12.h>
#include <cstdio>

static void print_module(const char *name)
{
	char path[MAX_PATH];
	HMODULE mod = GetModuleHandleA(name);

	if (mod && GetModuleFileNameA(mod, path, sizeof(path)))
		std::printf("module %s: %s\n", name, path);
	else
		std::printf("module %s: not loaded\n", name);
}

int main()
{
	ID3D12Device *device = nullptr;
	HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
	    __uuidof(ID3D12Device), reinterpret_cast<void **>(&device));

	std::printf("D3D12CreateDevice: 0x%08lx\n", (unsigned long)hr);
	print_module("d3d12.dll");
	print_module("d3d12core.dll");
	print_module("dxgi.dll");
	std::fflush(stdout);

	if (device)
		device->Release();
	return SUCCEEDED(hr) ? 0 : 1;
}
