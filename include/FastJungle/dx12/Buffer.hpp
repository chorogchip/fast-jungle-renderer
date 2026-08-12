#pragma once

#include "FastJungle/dx12/Resource.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"

#include <d3d12.h>
#include <source_location>
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
            D3D12_RESOURCE_STATES initial_state,
            std::source_location loc =
                std::source_location::current());

        [[nodiscard]]
        UINT64 get_byte_size() const noexcept {
            return resource_->GetDesc().Width;
        }

        template<typename T>
        [[nodiscard]]
        UINT get_element_count() const noexcept {
            static_assert(std::is_trivially_copyable_v<T>);
            return static_cast<UINT>(get_byte_size() / sizeof(T));
        }

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

        template<typename T>
        void create_structured_srv(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location) const {

            static_assert(std::is_trivially_copyable_v<T>);
            create_structured_srv(
                device,
                location,
                sizeof(T),
                0,
                get_element_count<T>());
        }

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
