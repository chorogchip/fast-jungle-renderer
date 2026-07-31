#pragma once

#include "FastJungle/dx12/DescriptorAllocator.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/Resource.hpp"

#include <d3d12.h>

namespace fjr::dx {

    enum class TextureType {
        texture2d,
        texture2d_array,
        texture_cube,
        texture_cube_array,
        texture3d
    };

    struct TextureViewRange {
        UINT first_mip = 0;
        UINT mip_count = 0;

        UINT first_slice = 0;
        UINT slice_count = 0;
    };

    class Texture : public Resource {
    public:
        Texture() = default;

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        Texture(Texture&&) noexcept = default;
        Texture& operator=(Texture&&) noexcept = default;

        void init(
            ID3D12Device* device,
            const D3D12_RESOURCE_DESC& description,
            TextureType type,
            D3D12_RESOURCE_STATES initial_state,
            const D3D12_CLEAR_VALUE* clear_value = nullptr);

        void attach(
            ID3D12Resource* resource,
            TextureType type,
            D3D12_RESOURCE_STATES initial_state);

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            const TextureViewRange& range = {},
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
            UINT descriptor_offset = 0) const;

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE create_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT mip_slice = 0,
            UINT first_slice = 0,
            UINT slice_count = 0,
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
            UINT descriptor_offset = 0) const;

        [[nodiscard]]
        D3D12_CPU_DESCRIPTOR_HANDLE create_rtv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT mip_slice = 0,
            UINT first_slice = 0,
            UINT slice_count = 0,
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
            UINT descriptor_offset = 0) const;

        [[nodiscard]]
        D3D12_CPU_DESCRIPTOR_HANDLE create_dsv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT mip_slice = 0,
            UINT first_slice = 0,
            UINT slice_count = 0,
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
            D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE,
            UINT descriptor_offset = 0) const;

    private:
        TextureType type_ = TextureType::texture2d;
    };

} // namespace fjr::dx