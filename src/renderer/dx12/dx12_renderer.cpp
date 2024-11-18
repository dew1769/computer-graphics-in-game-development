#include "dx12_renderer.h"

#include "utils/com_error_handler.h"
#include "utils/window.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>


void cg::renderer::dx12_renderer::init()
{
	model = std::make_shared<cg::world::model>();
	model->load_obj("path_to_model.obj");

	camera = std::make_shared<cg::world::camera>();
	camera->set_position({0.f, 5.f, -10.f});
	camera->set_theta(static_cast<float>(M_PI) / 4);
	camera->set_phi(static_cast<float>(M_PI) / 4);

	load_pipeline();
	load_assets();
}

void cg::renderer::dx12_renderer::destroy()
{
	wait_for_gpu();
	CloseHandle(fence_event);
}

void cg::renderer::dx12_renderer::update()
{
	static float angle = 0.f;
	angle += 0.01f;
	camera->set_theta(angle);
}

void cg::renderer::dx12_renderer::render()
{
	populate_command_list();
	command_queue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(command_list.GetAddressOf()));
	swap_chain->Present(1, 0);
	move_to_next_frame();
}

ComPtr<IDXGIFactory4> cg::renderer::dx12_renderer::get_dxgi_factory()
{
	UINT dxgi_factory_flags = 0;
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debug_controller;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller))))
	{
		debug_controller->EnableDebugLayer();
		dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif
	ComPtr<IDXGIFactory4> dxgi_factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dxgi_factory)));
	return dxgi_factory;
}

void cg::renderer::dx12_renderer::initialize_device(ComPtr<IDXGIFactory4>& dxgi_factory)
{
	ComPtr<IDXGIAdapter1> hardware_adapter;
	dxgi_factory->EnumAdapters1(0, &hardware_adapter);
	ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
}

void cg::renderer::dx12_renderer::create_direct_command_queue()
{
	D3D12_COMMAND_QUEUE_DESC queue_desc{};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));
}

void cg::renderer::dx12_renderer::create_swap_chain(ComPtr<IDXGIFactory4>& dxgi_factory)
{
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
	swap_chain_desc.BufferCount = frame_count;
	swap_chain_desc.Width = width;
	swap_chain_desc.Height = height;
	swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swap_chain1;
	ThrowIfFailed(dxgi_factory->CreateSwapChainForHwnd(
		command_queue.Get(),
		utils::window::get_hwnd(),
		&swap_chain_desc,
		nullptr,
		nullptr,
		&swap_chain1
	));
	ThrowIfFailed(swap_chain1.As(&swap_chain));
}

void cg::renderer::dx12_renderer::create_render_target_views()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
	rtv_heap_desc.NumDescriptors = frame_count;
	rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart());

	for (UINT i = 0; i < frame_count; i++)
	{
		ThrowIfFailed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
		rtv_handle.Offset(1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	}
}

void cg::renderer::dx12_renderer::create_depth_buffer()
{
	D3D12_RESOURCE_DESC depth_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	D3D12_HEAP_PROPERTIES heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	D3D12_CLEAR_VALUE depth_clear_value;
	depth_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
	depth_clear_value.DepthStencil.Depth = 1.0f;
	depth_clear_value.DepthStencil.Stencil = 0;

	ThrowIfFailed(device->CreateCommittedResource(
		&heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&depth_desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depth_clear_value,
		IID_PPV_ARGS(&depth_buffer)
	));
}

void cg::renderer::dx12_renderer::create_command_allocators()
{
	for (UINT i = 0; i < frame_count; i++)
	{
		ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocators[i])));
	}
}

void cg::renderer::dx12_renderer::create_command_list()
{
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocators[0].Get(), nullptr, IID_PPV_ARGS(&command_list)));
}


void cg::renderer::dx12_renderer::load_pipeline()
{
	ComPtr<IDXGIFactory4> dxgi_factory = get_dxgi_factory();
	initialize_device(dxgi_factory);
	create_direct_command_queue();
	create_swap_chain(dxgi_factory);
	create_render_target_views();
	create_command_allocators();
	create_command_list();
}

D3D12_STATIC_SAMPLER_DESC cg::renderer::dx12_renderer::get_sampler_descriptor()
{
	D3D12_STATIC_SAMPLER_DESC sampler_desc{};
	return sampler_desc;
}

void cg::renderer::dx12_renderer::create_root_signature(const D3D12_STATIC_SAMPLER_DESC* sampler_descriptors, UINT num_sampler_descriptors)
{
    D3D12_ROOT_SIGNATURE_DESC root_signature_desc{};
    root_signature_desc.NumParameters = 0;
    root_signature_desc.pParameters = nullptr;
    root_signature_desc.NumStaticSamplers = num_sampler_descriptors;
    root_signature_desc.pStaticSamplers = sampler_descriptors;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized_root_signature;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized_root_signature, &error_blob);
    if (FAILED(hr))
    {
        if (error_blob)
        {
            OutputDebugStringA((char*)error_blob->GetBufferPointer());
        }
        ThrowIfFailed(hr);
    }
    ThrowIfFailed(device->CreateRootSignature(0, serialized_root_signature->GetBufferPointer(), serialized_root_signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
}


std::filesystem::path cg::renderer::dx12_renderer::get_shader_path(const std::string& shader_name)
{
    return std::filesystem::path("shaders") / shader_name;
}

ComPtr<ID3DBlob> cg::renderer::dx12_renderer::compile_shader(const std::filesystem::path& shader_path, const std::string& entrypoint, const std::string& target)
{
    ComPtr<ID3DBlob> shader_blob;
    ComPtr<ID3DBlob> error_blob;
    HRESULT hr = D3DCompileFromFile(
        shader_path.wstring().c_str(),
        nullptr,
        nullptr,
        entrypoint.c_str(),
        target.c_str(),
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &shader_blob,
        &error_blob);
    if (FAILED(hr))
    {
        if (error_blob)
        {
            OutputDebugStringA((char*)error_blob->GetBufferPointer());
        }
        ThrowIfFailed(hr);
    }
    return shader_blob;
}

void cg::renderer::dx12_renderer::create_pso(const std::string& shader_name)
{
    auto vertex_shader = compile_shader(get_shader_path(shader_name + "_vs.hlsl"), "main", "vs_5_0");
    auto pixel_shader = compile_shader(get_shader_path(shader_name + "_ps.hlsl"), "main", "ps_5_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.InputLayout = { input_layout.data(), (UINT)input_layout.size() };
    pso_desc.pRootSignature = root_signature.Get();
    pso_desc.VS = CD3DX12_SHADER_BYTECODE(vertex_shader.Get());
    pso_desc.PS = CD3DX12_SHADER_BYTECODE(pixel_shader.Get());
    pso_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso_desc.DepthStencilState.DepthEnable = TRUE;
    pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso_desc.DepthStencilState.StencilEnable = FALSE;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso_desc.SampleDesc.Count = 1;

    ThrowIfFailed(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state)));
}

void cg::renderer::dx12_renderer::create_resource_on_upload_heap(ComPtr<ID3D12Resource>& resource, UINT size, const std::wstring& name)
{
    CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    ThrowIfFailed(device->CreateCommittedResource(
        &heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource)));

    resource->SetName(name.c_str());
}

void cg::renderer::dx12_renderer::create_resource_on_default_heap(ComPtr<ID3D12Resource>& resource, UINT size, const std::wstring& name, D3D12_RESOURCE_DESC* resource_descriptor)
{
    CD3DX12_HEAP_PROPERTIES heap_properties(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

    ThrowIfFailed(device->CreateCommittedResource(
        &heap_properties,
        D3D12_HEAP_FLAG_NONE,
        resource_descriptor,
        initial_state,
        nullptr,
        IID_PPV_ARGS(&resource)));

    resource->SetName(name.c_str());
}

void cg::renderer::dx12_renderer::copy_data(const void* buffer_data, UINT buffer_size, ComPtr<ID3D12Resource>& destination_resource)
{
    void* mapped_data = nullptr;
    CD3DX12_RANGE read_range(0, 0);
    ThrowIfFailed(destination_resource->Map(0, &read_range, &mapped_data));
    memcpy(mapped_data, buffer_data, buffer_size);
    destination_resource->Unmap(0, nullptr);
}

void cg::renderer::dx12_renderer::copy_data(const void* buffer_data, const UINT buffer_size, ComPtr<ID3D12Resource>& destination_resource, ComPtr<ID3D12Resource>& intermediate_resource, D3D12_RESOURCE_STATES state_after, int row_pitch, int slice_pitch)
{
    void* mapped_data = nullptr;
    CD3DX12_RANGE read_range(0, 0);
    ThrowIfFailed(intermediate_resource->Map(0, &read_range, &mapped_data));
    memcpy(mapped_data, buffer_data, buffer_size);
    intermediate_resource->Unmap(0, nullptr);

    command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        destination_resource.Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST));

    command_list->CopyTextureRegion(
        &CD3DX12_TEXTURE_COPY_LOCATION(destination_resource.Get(), 0),
        0, 0, 0,
        &CD3DX12_TEXTURE_COPY_LOCATION(intermediate_resource.Get(), 0),
        nullptr);

    command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        destination_resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        state_after));
}

D3D12_VERTEX_BUFFER_VIEW cg::renderer::dx12_renderer::create_vertex_buffer_view(const ComPtr<ID3D12Resource>& vertex_buffer, const UINT vertex_buffer_size)
{
    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
    vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
    vertex_buffer_view.SizeInBytes = vertex_buffer_size;
    vertex_buffer_view.StrideInBytes = sizeof(vertex);
    return vertex_buffer_view;
}

D3D12_INDEX_BUFFER_VIEW cg::renderer::dx12_renderer::create_index_buffer_view(const ComPtr<ID3D12Resource>& index_buffer, const UINT index_buffer_size)
{
    D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
    index_buffer_view.BufferLocation = index_buffer->GetGPUVirtualAddress();
    index_buffer_view.SizeInBytes = index_buffer_size;
    index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
    return index_buffer_view;
}

void cg::renderer::dx12_renderer::create_shader_resource_view(const ComPtr<ID3D12Resource>& texture, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handler)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = texture->GetDesc().Format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(texture.Get(), &srv_desc, cpu_handler);
}

void cg::renderer::dx12_renderer::create_constant_buffer_view(const ComPtr<ID3D12Resource>& buffer, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handler)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
    cbv_desc.BufferLocation = buffer->GetGPUVirtualAddress();
    cbv_desc.SizeInBytes = (UINT)buffer->GetDesc().Width;

    device->CreateConstantBufferView(&cbv_desc, cpu_handler);
}

void cg::renderer::dx12_renderer::load_assets()
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&cbv_heap)));

    create_resource_on_upload_heap(vertex_buffer, vertex_buffer_size, L"Vertex Buffer");
    create_resource_on_upload_heap(index_buffer, index_buffer_size, L"Index Buffer");

    copy_data(vertex_data, vertex_buffer_size, vertex_buffer);
    copy_data(index_data, index_buffer_size, index_buffer);

    vertex_buffer_view = create_vertex_buffer_view(vertex_buffer, vertex_buffer_size);
    index_buffer_view = create_index_buffer_view(index_buffer, index_buffer_size);

    create_constant_buffer_view(constant_buffer, cbv_heap->GetCPUDescriptorHandleForHeapStart());

    ThrowIfFailed(device->CreateFence(fence_values[frame_index], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fence_event == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}


void cg::renderer::dx12_renderer::populate_command_list()
{
    ThrowIfFailed(command_allocators[frame_index]->Reset());
    ThrowIfFailed(command_list->Reset(command_allocators[frame_index].Get(), pipeline_state.Get()));

    command_list->SetGraphicsRootSignature(root_signature.Get());
    command_list->SetDescriptorHeaps(1, cbv_heap.GetAddressOf());

    command_list->SetGraphicsRootDescriptorTable(0, cbv_heap->GetGPUDescriptorHandleForHeapStart());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        render_targets[frame_index].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    command_list->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart(), frame_index, rtv_descriptor_size);
    command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
    command_list->IASetIndexBuffer(&index_buffer_view);

    command_list->DrawIndexedInstanced(index_count, 1, 0, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        render_targets[frame_index].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    command_list->ResourceBarrier(1, &barrier);

    ThrowIfFailed(command_list->Close());
}

void cg::renderer::dx12_renderer::move_to_next_frame()
{
	const UINT64 current_fence_value = fence_values[frame_index];
	ThrowIfFailed(command_queue->Signal(fence.Get(), current_fence_value));
	frame_index = swap_chain->GetCurrentBackBufferIndex();
	if (fence->GetCompletedValue() < fence_values[frame_index])
	{
		ThrowIfFailed(fence->SetEventOnCompletion(fence_values[frame_index], fence_event));
		WaitForSingleObject(fence_event, INFINITE);
	}
	fence_values[frame_index] = current_fence_value + 1;
}

void cg::renderer::dx12_renderer::wait_for_gpu()
{
	ThrowIfFailed(command_queue->Signal(fence.Get(), fence_values[frame_index]));
	ThrowIfFailed(fence->SetEventOnCompletion(fence_values[frame_index], fence_event));
	WaitForSingleObject(fence_event, INFINITE);
}


void cg::renderer::descriptor_heap::create_heap(ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT number, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = number;
    heap_desc.Type = type;
    heap_desc.Flags = flags;

    ThrowIfFailed(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&descriptor_heap)));
}

D3D12_CPU_DESCRIPTOR_HANDLE cg::renderer::descriptor_heap::get_cpu_descriptor_handle(UINT index) const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptor_heap->GetCPUDescriptorHandleForHeapStart(), index, descriptor_size);
}

D3D12_GPU_DESCRIPTOR_HANDLE cg::renderer::descriptor_heap::get_gpu_descriptor_handle(UINT index) const
{
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptor_heap->GetGPUDescriptorHandleForHeapStart(), index, descriptor_size);
}
ID3D12DescriptorHeap* cg::renderer::descriptor_heap::get() const
{
    return descriptor_heap.Get();
}

