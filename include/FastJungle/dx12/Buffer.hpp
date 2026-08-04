#pragma once

#include "FastJungle/dx12/Resource.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"

#include <d3d12.h>
#include <type_traits>

namespace fjr::dx {

    class Buffer : public Resource {
    public:
        Buffer() = default;

        Buffer(const Buffer&) = default;
        Buffer& operator=(const Buffer&) = default;

        Buffer(Buffer&&) noexcept = default;
        Buffer& operator=(Buffer&&) noexcept = default;

        void init(
            ID3D12Device* device,
            UINT64 size,
            D3D12_HEAP_TYPE heap_type,
            D3D12_RESOURCE_FLAGS flags,
            D3D12_RESOURCE_STATES initial_state);

        void create_typed_srv(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            DXGI_FORMAT format,
            UINT64 element_offset,
            UINT count) const;

        void create_structured_srv(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            UINT structure_stride,
            UINT64 element_offset,
            UINT count) const;

        void create_raw_srv(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            UINT64 element_offset,
            UINT count) const;

        void create_typed_uav(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            DXGI_FORMAT format,
            UINT64 element_offset,
            UINT count) const;

        void create_structured_uav(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            UINT structure_stride,
            UINT64 element_offset,
            UINT count,
            ID3D12Resource* counter_resource,
            UINT64 counter_offset) const;

        void create_raw_uav(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            UINT64 element_offset,
            UINT count) const;
    };

} // namespace fjr::dx