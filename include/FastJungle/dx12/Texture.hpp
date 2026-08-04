#pragma once

#include "FastJungle/dx12/View.hpp"
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

        Texture(const Texture&) = default;
        Texture& operator=(const Texture&) = default;
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

        void create_srv(
            ID3D12Device* device,
            D3D12_CPU_DESCRIPTOR_HANDLE location,
            const TextureViewRange& range,
            DXGI_FORMAT format) const;

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

    private:
        TextureType type_ = TextureType::texture2d;
    };

} // namespace fjr::dx