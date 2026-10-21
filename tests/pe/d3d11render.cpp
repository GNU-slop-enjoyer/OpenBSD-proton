/*
 * D3D11 rendering test: render offscreen and check the pixels.
 *
 * Creates a hardware D3D11 device, clears a 64x64 RGBA8 render target to
 * blue, draws a red triangle covering the left half of the viewport with
 * shaders compiled at run time (D3DCompile from d3dcompiler_47.dll), copies
 * the result to a staging texture and reads it back.  No window or swap
 * chain is involved, so it runs under Xvfb.  Output is TAP; the exit status
 * is the number of failed checks.  Needs a mingw-w64 C++ toolchain.
 */
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>

static int failures, test_num;

static void ok(bool cond, const char *what)
{
	test_num++;
	if (!cond)
		failures++;
	std::printf("%sok %d - %s\n", cond ? "" : "not ", test_num, what);
	std::fflush(stdout);
}

static const char shader_src[] =
	"float4 vs(uint id : SV_VertexID) : SV_Position\n"
	"{\n"
	"	/* edge on x = 0; covers -1 <= x <= 0, -1 <= y <= 1 */\n"
	"	float2 p[3] = { float2(0, -3), float2(0, 3), float2(-3, 0) };\n"
	"	return float4(p[id], 0, 1);\n"
	"}\n"
	"float4 ps() : SV_Target { return float4(1, 0, 0, 1); }\n";

typedef HRESULT (WINAPI *compile_fn)(const void *, SIZE_T, const char *,
    const D3D_SHADER_MACRO *, ID3DInclude *, const char *, const char *,
    UINT, UINT, ID3DBlob **, ID3DBlob **);

static ID3DBlob *compile(compile_fn D3DCompile, const char *entry,
    const char *target)
{
	ID3DBlob *code = nullptr, *errors = nullptr;
	HRESULT hr = D3DCompile(shader_src, sizeof(shader_src) - 1, "test",
	    nullptr, nullptr, entry, target, 0, 0, &code, &errors);

	if (FAILED(hr) && errors)
		std::printf("# %s: %s\n", entry,
		    (const char *)errors->GetBufferPointer());
	if (errors)
		errors->Release();
	return SUCCEEDED(hr) ? code : nullptr;
}

int main()
{
	const UINT size = 64;
	ID3D11Device *dev = nullptr;
	ID3D11DeviceContext *ctx = nullptr;
	D3D_FEATURE_LEVEL level;
	HRESULT hr;

	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
	    nullptr, 0, D3D11_SDK_VERSION, &dev, &level, &ctx);
	ok(SUCCEEDED(hr), "D3D11CreateDevice");
	if (FAILED(hr)) {
		std::printf("# hr 0x%08lx\n", (unsigned long)hr);
		return failures;
	}
	std::printf("# feature level 0x%x\n", (unsigned)level);

	HMODULE dc = LoadLibraryA("d3dcompiler_47.dll");
	compile_fn D3DCompile = dc ? (compile_fn)(void *)GetProcAddress(dc,
	    "D3DCompile") : nullptr;
	ok(D3DCompile != nullptr, "D3DCompile available");
	if (!D3DCompile)
		return failures;
	ID3DBlob *vs_code = compile(D3DCompile, "vs", "vs_4_0");
	ID3DBlob *ps_code = compile(D3DCompile, "ps", "ps_4_0");
	ok(vs_code && ps_code, "shaders compiled");
	if (!vs_code || !ps_code)
		return failures;

	ID3D11VertexShader *vs = nullptr;
	ID3D11PixelShader *ps = nullptr;
	hr = dev->CreateVertexShader(vs_code->GetBufferPointer(),
	    vs_code->GetBufferSize(), nullptr, &vs);
	HRESULT hr2 = dev->CreatePixelShader(ps_code->GetBufferPointer(),
	    ps_code->GetBufferSize(), nullptr, &ps);
	ok(SUCCEEDED(hr) && SUCCEEDED(hr2), "shader objects created");

	D3D11_TEXTURE2D_DESC td = {};
	td.Width = td.Height = size;
	td.MipLevels = td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_RENDER_TARGET;
	ID3D11Texture2D *rt = nullptr, *staging = nullptr;
	ID3D11RenderTargetView *rtv = nullptr;
	hr = dev->CreateTexture2D(&td, nullptr, &rt);
	if (SUCCEEDED(hr))
		hr = dev->CreateRenderTargetView(rt, nullptr, &rtv);
	td.Usage = D3D11_USAGE_STAGING;
	td.BindFlags = 0;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	if (SUCCEEDED(hr))
		hr = dev->CreateTexture2D(&td, nullptr, &staging);
	ok(SUCCEEDED(hr), "render target and staging texture created");
	if (FAILED(hr) || !vs || !ps)
		return failures;

	const float blue[4] = { 0, 0, 1, 1 };
	D3D11_VIEWPORT vp = { 0, 0, (float)size, (float)size, 0, 1 };
	ctx->OMSetRenderTargets(1, &rtv, nullptr);
	ctx->RSSetViewports(1, &vp);
	ctx->ClearRenderTargetView(rtv, blue);
	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthClipEnable = TRUE;
	ID3D11RasterizerState *rs = nullptr;
	if (SUCCEEDED(dev->CreateRasterizerState(&rd, &rs)))
		ctx->RSSetState(rs);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->VSSetShader(vs, nullptr, 0);
	ctx->PSSetShader(ps, nullptr, 0);
	ctx->Draw(3, 0);
	ctx->CopyResource(staging, rt);

	D3D11_MAPPED_SUBRESOURCE map;
	hr = ctx->Map(staging, 0, D3D11_MAP_READ, 0, &map);
	ok(SUCCEEDED(hr), "staging texture mapped");
	if (FAILED(hr))
		return failures;
	auto pixel = [&](UINT x, UINT y) {
		UINT32 v;
		std::memcpy(&v, (const BYTE *)map.pData + y * map.RowPitch +
		    x * 4, 4);
		return v;
	};
	UINT32 left = pixel(size / 4, size / 2);
	UINT32 right = pixel(size * 3 / 4, size / 2);
	ctx->Unmap(staging, 0);
	std::printf("# left 0x%08x right 0x%08x (ABGR)\n", (unsigned)left,
	    (unsigned)right);
	ok(left == 0xff0000ff, "left half drawn red");
	ok(right == 0xffff0000, "right half cleared blue");

	return failures;
}
