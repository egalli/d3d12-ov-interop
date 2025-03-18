
#include <exception>
#include <iostream>
#include <memory>
#include <openvino/runtime/tensor.hpp>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d11on12.h>
#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>

#include <openvino/openvino.hpp>
#include <openvino/runtime/intel_gpu/ocl/dx.hpp>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

using Microsoft::WRL::ComPtr;

int main() {
  std::string dirname(__FILE__);
  dirname = dirname.substr(0, dirname.find_last_of('\\') + 1);
  std::string modelPath = dirname + "add_mul.xml";
  std::cout << "Model path: " << modelPath << std::endl;

  // Create D3D12 Device

  ComPtr<ID3D12Device> d3d12Device;
  if (FAILED(::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&d3d12Device)))) {
    std::cerr << "Failed to create D3D12 device" << std::endl;
    return 1;
  }
  ComPtr<ID3D11On12Device> d3d11on12Device;
  ComPtr<ID3D11Device> d3d11Device;
  if (FAILED(::D3D11On12CreateDevice(
          d3d12Device.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
          nullptr, 0, 0, &d3d11Device, nullptr, nullptr))) {
    std::cerr << "Failed to create D3D11On12 device" << std::endl;
    return 1;
  }
  if (FAILED(d3d11Device.As(&d3d11on12Device))) {
    std::cerr << "Failed to get D3D11On12 device" << std::endl;
    return 1;
  }

  ComPtr<ID3D12Resource> dest_buffer;
  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Alignment = 0;
  desc.Width = 3 * 3 * sizeof(float);
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.Format = DXGI_FORMAT_UNKNOWN;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
  if (FAILED(d3d12Device->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
          nullptr, IID_PPV_ARGS(&dest_buffer)))) {
    std::cerr << "Failed to create D3D12 resource" << std::endl;
    return 1;
  }
  ComPtr<ID3D11Buffer> d3d11Buffer;
  D3D11_RESOURCE_FLAGS flags = {D3D11_BIND_UNORDERED_ACCESS};
  if (FAILED(d3d11on12Device->CreateWrappedResource(
          dest_buffer.Get(), &flags, D3D12_RESOURCE_STATE_COMMON,
          D3D12_RESOURCE_STATE_COMMON, IID_PPV_ARGS(&d3d11Buffer)))) {
    std::cerr << "Failed to create D3D11 buffer" << std::endl;
    return 1;
  }

  try {

    ov::Core core;
    ov::intel_gpu::ocl::D3DContext context(core, d3d11Device.Get());
    auto model = core.read_model(modelPath);

    std::vector<float> data_a(3 * 3, 1);
    std::vector<float> data_b(3 * 3, 2);
    std::vector<float> data_d(3 * 3, 1);

    auto compiled = core.compile_model(model, context);
    auto infer_request = compiled.create_infer_request();
    ov::Tensor a(ov::element::f32, {1, 1, 3, 3}, data_a.data());
    ov::Tensor b(ov::element::f32, {1, 1, 3, 3}, data_b.data());
    ov::Tensor d(ov::element::f32, {1, 1, 3, 3}, data_d.data());

    auto dest = context.create_tensor(ov::element::f32, {1, 1, 3, 3},
                                      d3d11Buffer.Get());

    infer_request.set_input_tensor(0, a);
    infer_request.set_input_tensor(1, b);
    infer_request.set_input_tensor(2, d);
    infer_request.set_output_tensor(0, dest);
    infer_request.infer();
    // auto output_tensor = infer_request.get_output_tensor();
    // std::cout << "Output tensor shape: " << output_tensor.get_shape()
    //           << std::endl;
    // std::cout << "Output tensor data: ";
    // for (size_t i = 0; i < output_tensor.get_size(); i++) {
    //   std::cout << output_tensor.data<float>()[i] << " ";
    // }
    // std::cout << std::endl;

  } catch (const std::exception &ex) {
    std::cerr << ex.what() << std::endl;
    return 1;
  }

  ComPtr<ID3D12CommandQueue> command_queue;
  D3D12_COMMAND_QUEUE_DESC queue_desc = {};
  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (FAILED(d3d12Device->CreateCommandQueue(&queue_desc,
                                             IID_PPV_ARGS(&command_queue)))) {
    std::cerr << "Failed to create D3D12 command queue" << std::endl;
    return 1;
  }

  // Create readback buffer
  ComPtr<ID3D12Resource> readback_buffer;
  heapProps.Type = D3D12_HEAP_TYPE_READBACK;
  if (FAILED(d3d12Device->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
          D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
          IID_PPV_ARGS(&readback_buffer)))) {
    std::cerr << "Failed to create D3D12 readback resource" << std::endl;
    return 1;
  }

  ComPtr<ID3D12CommandAllocator> command_allocator;
  if (FAILED(d3d12Device->CreateCommandAllocator(
          D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)))) {
    std::cerr << "Failed to create D3D12 command allocator" << std::endl;
    return 1;
  }

  ComPtr<ID3D12GraphicsCommandList> command_list;
  if (FAILED(d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            command_allocator.Get(), nullptr,
                                            IID_PPV_ARGS(&command_list)))) {
    std::cerr << "Failed to create D3D12 command list" << std::endl;
    return 1;
  }
  command_list->CopyResource(readback_buffer.Get(), dest_buffer.Get());
  if (FAILED(command_list->Close())) {
    std::cerr << "Failed to close D3D12 command list" << std::endl;
    return 1;
  }
  command_queue->ExecuteCommandLists(
      1, reinterpret_cast<ID3D12CommandList *const *>(
             command_list.GetAddressOf()));
  
  //Create fence
  ComPtr<ID3D12Fence> fence;
  if (FAILED(d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
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
  void *mapped_data;
  if (FAILED(readback_buffer->Map(0, nullptr, &mapped_data))) {
    std::cerr << "Failed to map D3D12 readback buffer" << std::endl;
    return 1;
  }
  float *data = static_cast<float *>(mapped_data);
  std::cout << "Output tensor data: ";
  for (size_t i = 0; i < 3 * 3; i++) {
    std::cout << data[i] << " ";
  }
  std::cout << std::endl;
  readback_buffer->Unmap(0, nullptr);

  return 0;
}
