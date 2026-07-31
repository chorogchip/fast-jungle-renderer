#include "FastJungle/dx12/DescriptorHeap.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::dx {

    void DescriptorHeap::init(
        ID3D12Device* device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        UINT capacity,
        bool shader_visible) {

        descriptor_heap_.Reset();

        type_ = type;
        capacity_ = capacity;
        shader_visible_ = shader_visible;

        D3D12_DESCRIPTOR_HEAP_DESC description{};
        description.Type = type_;
        description.NumDescriptors = capacity_;
        description.Flags = shader_visible_
            ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
            : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        description.NodeMask = 0;

        abort_failed(device->CreateDescriptorHeap(
            &description,
            IID_PPV_ARGS(descriptor_heap_.ReleaseAndGetAddressOf())));

        descriptor_size_ =
            device->GetDescriptorHandleIncrementSize(type_);

        cpu_start_ =
            descriptor_heap_->GetCPUDescriptorHandleForHeapStart();

        gpu_start_ = shader_visible_
            ? descriptor_heap_->GetGPUDescriptorHandleForHeapStart()
            : D3D12_GPU_DESCRIPTOR_HANDLE{};
    }

} // namespace fjr::dx