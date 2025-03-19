
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define NOMINMAX
#include <d3d11on12.h>
#include <d3d12.h>
#include <d3d12compatibility.h>
#include <windows.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

using Microsoft::WRL::ComPtr;

int main() {
	{
		ComPtr<ID3D12Debug> debug_controller;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) {
			debug_controller->EnableDebugLayer();
		}
	}

	// Create D3D12 Device
	ComPtr<ID3D12Device> d3d12_device;
	if (FAILED(::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&d3d12_device)))) {
		std::cerr << "Failed to create D3D12 device" << std::endl;
		return 1;
	}

	ComPtr<ID3D12CompatibilityDevice> compatibility_device;
	if (FAILED(d3d12_device.As(&compatibility_device))) {
		std::cerr << "Failed to get D3D12CompatibilityDevice" << std::endl;
		return 1;
	}

	ComPtr<ID3D12CommandQueue> command_queue;
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (FAILED(d3d12_device->CreateCommandQueue(&queue_desc,
		IID_PPV_ARGS(&command_queue)))) {
		std::cerr << "Failed to create D3D12 command queue" << std::endl;
		return 1;
	}
	std::cout << "D3D12 device created" << std::endl;

	ComPtr<ID3D11On12Device2> d3d11on12_device;
	ComPtr<ID3D11Device> d3d11_device;
	if (FAILED(::D3D11On12CreateDevice(
		d3d12_device.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG, nullptr, 0,
		reinterpret_cast<IUnknown* const*>(command_queue.GetAddressOf()), 1,
		1, &d3d11_device, nullptr, nullptr))) {
		std::cerr << "Failed to create D3D11On12 device" << std::endl;
		return 1;
	}
	std::cout << "D3D11 device created" << std::endl;
	if (FAILED(d3d11_device.As(&d3d11on12_device))) {
		std::cerr << "Failed to get D3D11On12 device" << std::endl;
		return 1;
	}
	std::cout << "D3D11On12 device created" << std::endl;

	ComPtr<ID3D12Resource> d3d12_buffer;
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = 3 * 3 * sizeof(float);
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D11_RESOURCE_FLAGS flags = {};
	flags.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	flags.MiscFlags = 0
        | D3D11_RESOURCE_MISC_SHARED
		| D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	//// Create from D3D12 device
	//if (FAILED(d3d12_device->CreateCommittedResource(
	//	&heap_props, D3D12_HEAP_FLAG_NONE, &desc,
	//	D3D12_RESOURCE_STATE_COMMON, nullptr,
	//	IID_PPV_ARGS(&d3d12_buffer)))) {
	//	std::cerr << "Failed to create D3D12 resource" << std::endl;
	//	return 1;
	//}

	 // Create using compatibility device
	 if (FAILED(compatibility_device->CreateSharedResource(
	         &heap_props, D3D12_HEAP_FLAG_SHARED, &desc,
	         D3D12_RESOURCE_STATE_COMMON, nullptr, &flags,
	         D3D12_COMPATIBILITY_SHARED_FLAG_NON_NT_HANDLE, nullptr, nullptr,
	         IID_PPV_ARGS(&d3d12_buffer)))) {
	   std::cerr << "Failed to create shared D3D12 resource" << std::endl;
	   return 1;
	 }

	ComPtr<ID3D11Buffer> d3d11_buffer;
	if (FAILED(d3d11on12_device->CreateWrappedResource(
		d3d12_buffer.Get(), &flags, D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_COMMON, IID_PPV_ARGS(&d3d11_buffer)))) {
		std::cerr << "Failed to create D3D11 buffer" << std::endl;

		return 1;
	}

	// // Create from D3D11 device
	// ComPtr<ID3D11Buffer> d3d11_buffer;
	// {
	//   D3D11_BUFFER_DESC desc = {};
	//   desc.ByteWidth = 3 * 3 * sizeof(float);
	//   desc.Usage = D3D11_USAGE_DEFAULT;
	//   desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	//   desc.CPUAccessFlags = 0;
	//   desc.MiscFlags =
	//       D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS | D3D11_RESOURCE_MISC_SHARED;
	//   if (FAILED(d3d11_device->CreateBuffer(&desc, nullptr, &d3d11_buffer))) {
	//     std::cerr << "Failed to create D3D11 buffer" << std::endl;
	//     return 1;
	//   }
	// }

	d3d11on12_device->ReleaseWrappedResources(
		reinterpret_cast<ID3D11Resource* const*>(d3d11_buffer.GetAddressOf()),
		1);

	// if (FAILED(d3d11on12_device->UnwrapUnderlyingResource(
	//         d3d11_buffer.Get(), command_queue.Get(),
	//         IID_PPV_ARGS(&d3d12_buffer)))) {
	//   std::cerr << "Failed to unwrap D3D11 buffer" << std::endl;
	//   return 1;
	// }

	// Create readback buffer
	ComPtr<ID3D12Resource> readback_buffer;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	heap_props.Type = D3D12_HEAP_TYPE_READBACK;
	if (FAILED(d3d12_device->CreateCommittedResource(
		&heap_props, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&readback_buffer)))) {
		std::cerr << "Failed to create D3D12 readback resource" << std::endl;
		return 1;
	}

	ComPtr<ID3D12CommandAllocator> command_allocator;
	if (FAILED(d3d12_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)))) {
		std::cerr << "Failed to create D3D12 command allocator" << std::endl;
		return 1;
	}

	ComPtr<ID3D12GraphicsCommandList> command_list;
	if (FAILED(d3d12_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		command_allocator.Get(), nullptr,
		IID_PPV_ARGS(&command_list)))) {
		std::cerr << "Failed to create D3D12 command list" << std::endl;
		return 1;
	}

	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = d3d12_buffer.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		command_list->ResourceBarrier(1, &barrier);
	}

	command_list->CopyResource(readback_buffer.Get(), d3d12_buffer.Get());
	if (FAILED(command_list->Close())) {
		std::cerr << "Failed to close D3D12 command list" << std::endl;
		return 1;
	}
	command_queue->ExecuteCommandLists(
		1, reinterpret_cast<ID3D12CommandList* const*>(
			command_list.GetAddressOf()));

	// Create fence
	ComPtr<ID3D12Fence> fence;
	if (FAILED(d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&fence)))) {
		std::cerr << "Failed to create D3D12 fence" << std::endl;
		return 1;
	}
	HANDLE fence_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr) {
		std::cerr << "Failed to create fence event" << std::endl;
		return 1;
	}
	command_queue->Signal(fence.Get(), 1);
	if (fence->GetCompletedValue() < 1) {
		fence->SetEventOnCompletion(1, fence_event);
		::WaitForSingleObject(fence_event, INFINITE);
	}
	// Map readback buffer
	void* mapped_data;
	if (FAILED(readback_buffer->Map(0, nullptr, &mapped_data))) {
		std::cerr << "Failed to map D3D12 readback buffer" << std::endl;
		return 1;
	}
	float* data = static_cast<float*>(mapped_data);
	std::cout << "Output tensor data: ";
	for (size_t i = 0; i < 3 * 3; i++) {
		std::cout << data[i] << " ";
	}
	std::cout << std::endl;
	D3D12_RANGE empty = {};
	readback_buffer->Unmap(0, &empty);

	return 0;
}