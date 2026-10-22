/*
 * D3D12 rendering test: render offscreen and check the pixels.
 *
 * Creates a D3D12 device (vkd3d-proton under proton-openbsd), clears a
 * 64x64 RGBA8 render target to blue, draws a red triangle covering the left
 * half with shaders compiled at run time (D3DCompile, shader model 5.0),
 * copies the target to a readback buffer and checks two pixels.  No window
 * or swap chain, so it runs under Xvfb.  Output is TAP; the exit status is
 * the number of failed checks.  Needs a mingw-w64 C++ toolchain.
 */
#include <windows.h>
#include <d3d12.h>
#include <d3dcommon.h>
#include <cstdio>
#include <cstring>

static int checks, failures;

static void ok(bool cond, const char *what)
{
	checks++;
	if (!cond)
		failures++;
	std::printf("%s %d - %s\n", cond ? "ok" : "not ok", checks, what);
}

/* a triangle strip over the left half: x in [-1, 0], y in [-1, 1] */
static const char shader_src[] =
    "float4 vs(uint id : SV_VertexID) : SV_Position {\n"
    "    return float4((id & 2) ? 0.0 : -1.0, (id & 1) ? 1.0 : -1.0, 0, 1);\n"
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

	if (FAILED(hr)) {
		std::printf("# %s: 0x%08lx %s\n", entry, (unsigned long)hr,
		    errors ? (const char *)errors->GetBufferPointer() : "");
		return nullptr;
	}
	return code;
}

static void transition(ID3D12GraphicsCommandList *list, ID3D12Resource *res,
    D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
	D3D12_RESOURCE_BARRIER b = {};

	b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	b.Transition.pResource = res;
	b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	b.Transition.StateBefore = from;
	b.Transition.StateAfter = to;
	list->ResourceBarrier(1, &b);
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);	/* keep the output if the program crashes */
	const UINT size = 64;
	ID3D12Device *dev = nullptr;
	HRESULT hr;

	hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
	    __uuidof(ID3D12Device), (void **)&dev);
	ok(SUCCEEDED(hr), "D3D12CreateDevice");
	if (FAILED(hr)) {
		std::printf("# hr 0x%08lx\n", (unsigned long)hr);
		return failures;
	}

	HMODULE dc = LoadLibraryA("d3dcompiler_47.dll");
	compile_fn D3DCompile = dc ? (compile_fn)(void *)GetProcAddress(dc,
	    "D3DCompile") : nullptr;
	ok(D3DCompile != nullptr, "D3DCompile available");
	if (!D3DCompile)
		return failures;
	ID3DBlob *vs_code = compile(D3DCompile, "vs", "vs_5_0");
	ID3DBlob *ps_code = compile(D3DCompile, "ps", "ps_5_0");
	ok(vs_code && ps_code, "shaders compiled");
	if (!vs_code || !ps_code)
		return failures;

	/* empty root signature */
	D3D12_ROOT_SIGNATURE_DESC rsd = {};
	ID3DBlob *rs_blob = nullptr, *rs_err = nullptr;
	ID3D12RootSignature *rs = nullptr;
	hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1,
	    &rs_blob, &rs_err);
	if (SUCCEEDED(hr))
		hr = dev->CreateRootSignature(0, rs_blob->GetBufferPointer(),
		    rs_blob->GetBufferSize(), __uuidof(ID3D12RootSignature),
		    (void **)&rs);
	ok(SUCCEEDED(hr), "root signature created");
	if (FAILED(hr))
		return failures;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
	pd.pRootSignature = rs;
	pd.VS = { vs_code->GetBufferPointer(), vs_code->GetBufferSize() };
	pd.PS = { ps_code->GetBufferPointer(), ps_code->GetBufferSize() };
	pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pd.SampleMask = 0xffffffff;
	pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pd.RasterizerState.DepthClipEnable = TRUE;
	pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pd.NumRenderTargets = 1;
	pd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pd.SampleDesc.Count = 1;
	ID3D12PipelineState *pso = nullptr;
	hr = dev->CreateGraphicsPipelineState(&pd, __uuidof(ID3D12PipelineState),
	    (void **)&pso);
	ok(SUCCEEDED(hr), "pipeline state created");
	if (FAILED(hr)) {
		std::printf("# hr 0x%08lx\n", (unsigned long)hr);
		return failures;
	}

	D3D12_COMMAND_QUEUE_DESC qd = {};
	qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ID3D12CommandQueue *queue = nullptr;
	ID3D12CommandAllocator *alloc = nullptr;
	ID3D12GraphicsCommandList *list = nullptr;
	hr = dev->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void **)&queue);
	if (SUCCEEDED(hr))
		hr = dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		    __uuidof(ID3D12CommandAllocator), (void **)&alloc);
	if (SUCCEEDED(hr))
		hr = dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc,
		    pso, __uuidof(ID3D12GraphicsCommandList), (void **)&list);
	ok(SUCCEEDED(hr), "command queue and list created");
	if (FAILED(hr))
		return failures;

	/* render target */
	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC td = {};
	td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	td.Width = size;
	td.Height = size;
	td.DepthOrArraySize = 1;
	td.MipLevels = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	D3D12_CLEAR_VALUE cv = {};
	cv.Format = td.Format;
	cv.Color[2] = 1.0f;
	cv.Color[3] = 1.0f;
	ID3D12Resource *rt = nullptr;
	hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td,
	    D3D12_RESOURCE_STATE_RENDER_TARGET, &cv, __uuidof(ID3D12Resource),
	    (void **)&rt);

	/* readback buffer, rows aligned to 256 bytes */
	const UINT pitch = (size * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) &
	    ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
	D3D12_HEAP_PROPERTIES rp = {};
	rp.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC bd = {};
	bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bd.Width = pitch * size;
	bd.Height = 1;
	bd.DepthOrArraySize = 1;
	bd.MipLevels = 1;
	bd.SampleDesc.Count = 1;
	bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ID3D12Resource *readback = nullptr;
	if (SUCCEEDED(hr))
		hr = dev->CreateCommittedResource(&rp, D3D12_HEAP_FLAG_NONE, &bd,
		    D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		    __uuidof(ID3D12Resource), (void **)&readback);
	ok(SUCCEEDED(hr), "render target and readback buffer created");
	if (FAILED(hr))
		return failures;

	D3D12_DESCRIPTOR_HEAP_DESC hd = {};
	hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	hd.NumDescriptors = 1;
	ID3D12DescriptorHeap *rtv_heap = nullptr;
	hr = dev->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap),
	    (void **)&rtv_heap);
	if (FAILED(hr))
		return failures + 1;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtv_heap->GetCPUDescriptorHandleForHeapStart();
	dev->CreateRenderTargetView(rt, nullptr, rtv);

	D3D12_VIEWPORT vp = { 0, 0, (float)size, (float)size, 0, 1 };
	D3D12_RECT sc = { 0, 0, (LONG)size, (LONG)size };
	list->ClearRenderTargetView(rtv, cv.Color, 0, nullptr);
	list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	list->SetGraphicsRootSignature(rs);
	list->RSSetViewports(1, &vp);
	list->RSSetScissorRects(1, &sc);
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	list->DrawInstanced(4, 1, 0, 0);
	transition(list, rt, D3D12_RESOURCE_STATE_RENDER_TARGET,
	    D3D12_RESOURCE_STATE_COPY_SOURCE);

	D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};
	dst.pResource = readback;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.PlacedFootprint.Footprint.Format = td.Format;
	dst.PlacedFootprint.Footprint.Width = size;
	dst.PlacedFootprint.Footprint.Height = size;
	dst.PlacedFootprint.Footprint.Depth = 1;
	dst.PlacedFootprint.Footprint.RowPitch = pitch;
	src.pResource = rt;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;
	list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	hr = list->Close();
	ok(SUCCEEDED(hr), "command list recorded");
	if (FAILED(hr))
		return failures;

	ID3D12CommandList *lists[] = { list };
	queue->ExecuteCommandLists(1, lists);
	ID3D12Fence *fence = nullptr;
	hr = dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
	    (void **)&fence);
	HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
	if (SUCCEEDED(hr))
		hr = queue->Signal(fence, 1);
	if (SUCCEEDED(hr) && fence->GetCompletedValue() < 1) {
		fence->SetEventOnCompletion(1, ev);
		WaitForSingleObject(ev, 10000);
	}
	ok(SUCCEEDED(hr) && fence->GetCompletedValue() == 1, "GPU work completed");

	void *data = nullptr;
	D3D12_RANGE range = { 0, (SIZE_T)pitch * size };
	hr = readback->Map(0, &range, &data);
	ok(SUCCEEDED(hr), "readback buffer mapped");
	if (FAILED(hr))
		return failures;
	const UINT *row = (const UINT *)((const char *)data + pitch * (size / 2));
	UINT left = row[size / 4], right = row[size * 3 / 4];
	std::printf("# left 0x%08x right 0x%08x (ABGR)\n", left, right);
	ok(left == 0xff0000ff, "left half drawn red");
	ok(right == 0xffff0000, "right half cleared blue");
	readback->Unmap(0, nullptr);
	return failures;
}
