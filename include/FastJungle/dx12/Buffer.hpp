#pragma once

#include "FastJungle/dx12/DescriptorAllocator.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/Resource.hpp"

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

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_typed_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            DXGI_FORMAT format,
            UINT64 first_element = 0,
            UINT element_count = 0,
            UINT descriptor_offset = 0) const;

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_structured_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT structure_stride,
            UINT64 first_element = 0,
            UINT element_count = 0,
            UINT descriptor_offset = 0) const;

        template<typename T>
        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_structured_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT64 first_element = 0,
            UINT element_count = 0,
            UINT descriptor_offset = 0) const {

            static_assert(std::is_trivially_copyable_v<T>);

            return create_structured_srv(
                device,
                heap,
                allocation,
                static_cast<UINT>(sizeof(T)),
                first_element,
                element_count,
                descriptor_offset);
        }

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_raw_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT64 first_byte = 0,
            UINT64 byte_size = 0,
            UINT descriptor_offset = 0) const;

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_typed_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            DXGI_FORMAT format,
            UINT64 first_element = 0,
            UINT element_count = 0,
            UINT descriptor_offset = 0) const;

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_structured_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT structure_stride,
            UINT64 first_element = 0,
            UINT element_count = 0,
            ID3D12Resource* counter_resource = nullptr,
            UINT64 counter_offset = 0,
            UINT descriptor_offset = 0) const;

        template<typename T>
        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_structured_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT64 first_element = 0,
            UINT element_count = 0,
            ID3D12Resource* counter_resource = nullptr,
            UINT64 counter_offset = 0,
            UINT descriptor_offset = 0) const {

            static_assert(std::is_trivially_copyable_v<T>);

            return create_structured_uav(
                device,
                heap,
                allocation,
                static_cast<UINT>(sizeof(T)),
                first_element,
                element_count,
                counter_resource,
                counter_offset,
                descriptor_offset);
        }

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_raw_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT64 first_byte = 0,
            UINT64 byte_size = 0,
            UINT descriptor_offset = 0) const;
    };

} // namespace fjr::dx