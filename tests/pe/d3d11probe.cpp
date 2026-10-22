/*
 * D3D11/DXGI probe: enumerate DXGI adapters and try to create a D3D11
 * device on the hardware driver, printing every result.  Used to check
 * which D3D implementation is loaded (wined3d or DXVK) and how far it gets
 * on a given machine.  Needs a mingw-w64 C++ toolchain.
 *
 * Exit status: 0 if a device was created, 1 otherwise.
 */
#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
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
	setvbuf(stdout, nullptr, _IONBF, 0);	/* keep the output if the program crashes */
	IDXGIFactory1 *factory = nullptr;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
	    reinterpret_cast<void **>(&factory));
	std::printf("CreateDXGIFactory1: 0x%08lx\n", (unsigned long)hr);
	print_module("dxgi.dll");

	unsigned adapters = 0;
	if (SUCCEEDED(hr)) {
		IDXGIAdapter1 *adapter;
		while (factory->EnumAdapters1(adapters, &adapter) == S_OK) {
			DXGI_ADAPTER_DESC1 desc;
			if (SUCCEEDED(adapter->GetDesc1(&desc)))
				std::printf("adapter %u: %ls vendor 0x%04x "
				    "device 0x%04x vram %zu MiB\n", adapters,
				    desc.Description, desc.VendorId,
				    desc.DeviceId,
				    (size_t)(desc.DedicatedVideoMemory >> 20));
			adapter->Release();
			adapters++;
		}
		factory->Release();
	}
	std::printf("adapters: %u\n", adapters);

	ID3D11Device *device = nullptr;
	ID3D11DeviceContext *context = nullptr;
	D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_9_1;
	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
	    nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context);
	std::printf("D3D11CreateDevice: 0x%08lx", (unsigned long)hr);
	if (SUCCEEDED(hr))
		std::printf(" feature level 0x%x", (unsigned)level);
	std::printf("\n");
	print_module("d3d11.dll");

	if (context)
		context->Release();
	if (device)
		device->Release();
	return SUCCEEDED(hr) ? 0 : 1;
}
