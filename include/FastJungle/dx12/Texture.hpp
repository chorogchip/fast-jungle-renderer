#pragma once

#include "FastJungle/dx12/Resource.hpp"

#include <d3d12.h>
#include <source_location>

namespace fjr::dx {

    enum class TextureSrvType {
        texture,
        cube,
        cube_array
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

        Texture(const Texture&) = default;
        Texture& operator=(const Texture&) = default;
        Texture(Texture&&) noexcept = default;
        Texture& operator=(Texture&&) noexcept = default;

        void init(
            ID3D12Device* device,
            const D3D12_RESOURCE_DESC& description,
            D3D12_RESOURCE_STATES initial_state,
            const D3D12_CLEAR_VALUE* clear_value = nullptr,
            std::source_location loc =
                std::source_location::current());

        void attach(
            ID3D12Resource* resource,
            D3D12_RESOURCE_STATES initial_state);

        void create_srv(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            const TextureViewRange& range,
            DXGI_FORMAT format,
            TextureSrvType type = TextureSrvType::texture) const;

        void create_uav(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            UINT mip_slice,
            UINT first_slice,
            UINT slice_count,
            DXGI_FORMAT format) const;

        void create_rtv(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            UINT mip_slice,
            UINT first_slice,
            UINT slice_count,
            DXGI_FORMAT format) const;

        void create_dsv(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            UINT mip_slice,
            UINT first_slice,
            UINT slice_count,
            DXGI_FORMAT format,
            D3D12_DSV_FLAGS flags) const;

    };

} // namespace fjr::dx
