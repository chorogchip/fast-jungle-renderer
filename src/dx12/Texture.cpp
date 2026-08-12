#include "FastJungle/dx12/Texture.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

#include <utility>

namespace fjr::dx {

    void Texture::init(
        ID3D12Device* device,
        const D3D12_RESOURCE_DESC& description,
        D3D12_RESOURCE_STATES initial_state,
        const D3D12_CLEAR_VALUE* clear_value,
        std::source_location loc) {

        D3D12_HEAP_PROPERTIES heap_properties{};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heap_properties.CreationNodeMask = 1;
        heap_properties.VisibleNodeMask = 1;

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;

        abort_failed(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &description,
            initial_state,
            clear_value,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())),
            loc);

        set_resource(std::move(resource), initial_state);
    }

    void Texture::attach(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES initial_state) {

        set_resource(
            Microsoft::WRL::ComPtr<ID3D12Resource>(resource),
            initial_state);
    }

    void Texture::create_srv(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        const TextureViewRange& range,
        DXGI_FORMAT format,
        TextureSrvType type) const {

        const D3D12_RESOURCE_DESC resource = resource_->GetDesc();

        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format = format;
        description.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (type == TextureSrvType::cube) {
            description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            description.TextureCube.MostDetailedMip = range.first_mip;
            description.TextureCube.MipLevels = range.mip_count;
            description.TextureCube.ResourceMinLODClamp = 0.0f;
        }
        else if (type == TextureSrvType::cube_array) {
            description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
            description.TextureCubeArray.MostDetailedMip = range.first_mip;
            description.TextureCubeArray.MipLevels = range.mip_count;
            description.TextureCubeArray.First2DArrayFace = range.first_slice;
            description.TextureCubeArray.NumCubes = range.slice_count / 6;
            description.TextureCubeArray.ResourceMinLODClamp = 0.0f;
        }
        else {
            switch (resource.Dimension) {
            case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
                if (resource.DepthOrArraySize > 1) {
                    description.ViewDimension =
                        D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                    description.Texture1DArray.MostDetailedMip =
                        range.first_mip;
                    description.Texture1DArray.MipLevels = range.mip_count;
                    description.Texture1DArray.FirstArraySlice =
                        range.first_slice;
                    description.Texture1DArray.ArraySize = range.slice_count;
                    description.Texture1DArray.ResourceMinLODClamp = 0.0f;
                }
                else {
                    description.ViewDimension =
                        D3D12_SRV_DIMENSION_TEXTURE1D;
                    description.Texture1D.MostDetailedMip = range.first_mip;
                    description.Texture1D.MipLevels = range.mip_count;
                    description.Texture1D.ResourceMinLODClamp = 0.0f;
                }
                break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
                if (resource.SampleDesc.Count > 1) {
                    if (resource.DepthOrArraySize > 1) {
                        description.ViewDimension =
                            D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                        description.Texture2DMSArray.FirstArraySlice =
                            range.first_slice;
                        description.Texture2DMSArray.ArraySize =
                            range.slice_count;
                    }
                    else {
                        description.ViewDimension =
                            D3D12_SRV_DIMENSION_TEXTURE2DMS;
                    }
                }
                else if (resource.DepthOrArraySize > 1) {
                    description.ViewDimension =
                        D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    description.Texture2DArray.MostDetailedMip =
                        range.first_mip;
                    description.Texture2DArray.MipLevels = range.mip_count;
                    description.Texture2DArray.FirstArraySlice =
                        range.first_slice;
                    description.Texture2DArray.ArraySize = range.slice_count;
                    description.Texture2DArray.PlaneSlice = 0;
                    description.Texture2DArray.ResourceMinLODClamp = 0.0f;
                }
                else {
                    description.ViewDimension =
                        D3D12_SRV_DIMENSION_TEXTURE2D;
                    description.Texture2D.MostDetailedMip = range.first_mip;
                    description.Texture2D.MipLevels = range.mip_count;
                    description.Texture2D.PlaneSlice = 0;
                    description.Texture2D.ResourceMinLODClamp = 0.0f;
                }
                break;

            case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
                description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                description.Texture3D.MostDetailedMip = range.first_mip;
                description.Texture3D.MipLevels = range.mip_count;
                description.Texture3D.ResourceMinLODClamp = 0.0f;
                break;

            default:
                break;
            }
        }

        device->CreateShaderResourceView(resource_.Get(), &description, location);
    }

    void Texture::create_uav(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        UINT mip_slice,
        UINT first_slice,
        UINT slice_count,
        DXGI_FORMAT format) const {

        const D3D12_RESOURCE_DESC resource = resource_->GetDesc();

        D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
        description.Format = format;

        switch (resource.Dimension) {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            if (resource.DepthOrArraySize > 1) {
                description.ViewDimension =
                    D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                description.Texture1DArray.MipSlice = mip_slice;
                description.Texture1DArray.FirstArraySlice = first_slice;
                description.Texture1DArray.ArraySize = slice_count;
            }
            else {
                description.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                description.Texture1D.MipSlice = mip_slice;
            }
            break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            if (resource.DepthOrArraySize > 1) {
                description.ViewDimension =
                    D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                description.Texture2DArray.MipSlice = mip_slice;
                description.Texture2DArray.FirstArraySlice = first_slice;
                description.Texture2DArray.ArraySize = slice_count;
                description.Texture2DArray.PlaneSlice = 0;
            }
            else {
                description.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                description.Texture2D.MipSlice = mip_slice;
                description.Texture2D.PlaneSlice = 0;
            }
            break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            description.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            description.Texture3D.MipSlice = mip_slice;
            description.Texture3D.FirstWSlice = first_slice;
            description.Texture3D.WSize = slice_count;
            break;

        default:
            break;
        }

        device->CreateUnorderedAccessView(
            resource_.Get(),
            nullptr,
            &description,
            location);
    }

    void Texture::create_rtv(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        UINT mip_slice,
        UINT first_slice,
        UINT slice_count,
        DXGI_FORMAT format) const {

        const D3D12_RESOURCE_DESC resource = resource_->GetDesc();

        D3D12_RENDER_TARGET_VIEW_DESC description{};
        description.Format = format;

        switch (resource.Dimension) {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            if (resource.DepthOrArraySize > 1) {
                description.ViewDimension =
                    D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                description.Texture1DArray.MipSlice = mip_slice;
                description.Texture1DArray.FirstArraySlice = first_slice;
                description.Texture1DArray.ArraySize = slice_count;
            }
            else {
                description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                description.Texture1D.MipSlice = mip_slice;
            }
            break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            if (resource.SampleDesc.Count > 1) {
                if (resource.DepthOrArraySize > 1) {
                    description.ViewDimension =
                        D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                    description.Texture2DMSArray.FirstArraySlice = first_slice;
                    description.Texture2DMSArray.ArraySize = slice_count;
                }
                else {
                    description.ViewDimension =
                        D3D12_RTV_DIMENSION_TEXTURE2DMS;
                }
            }
            else if (resource.DepthOrArraySize > 1) {
                description.ViewDimension =
                    D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                description.Texture2DArray.MipSlice = mip_slice;
                description.Texture2DArray.FirstArraySlice = first_slice;
                description.Texture2DArray.ArraySize = slice_count;
                description.Texture2DArray.PlaneSlice = 0;
            }
            else {
                description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                description.Texture2D.MipSlice = mip_slice;
                description.Texture2D.PlaneSlice = 0;
            }
            break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
            description.Texture3D.MipSlice = mip_slice;
            description.Texture3D.FirstWSlice = first_slice;
            description.Texture3D.WSize = slice_count;
            break;

        default:
            break;
        }

        device->CreateRenderTargetView(resource_.Get(), &description, location);
    }

    void Texture::create_dsv(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        UINT mip_slice,
        UINT first_slice,
        UINT slice_count,
        DXGI_FORMAT format,
        D3D12_DSV_FLAGS flags) const {

        const D3D12_RESOURCE_DESC resource = resource_->GetDesc();

        D3D12_DEPTH_STENCIL_VIEW_DESC description{};
        description.Format = format;
        description.Flags = flags;

        switch (resource.Dimension) {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            if (resource.DepthOrArraySize > 1) {
                description.ViewDimension =
                    D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                description.Texture1DArray.MipSlice = mip_slice;
                description.Texture1DArray.FirstArraySlice = first_slice;
                description.Texture1DArray.ArraySize = slice_count;
            }
            else {
                description.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                description.Texture1D.MipSlice = mip_slice;
            }
            break;

        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
            if (resource.SampleDesc.Count > 1) {
                if (resource.DepthOrArraySize > 1) {
                    description.ViewDimension =
                        D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                    description.Texture2DMSArray.FirstArraySlice = first_slice;
                    description.Texture2DMSArray.ArraySize = slice_count;
                }
                else {
                    description.ViewDimension =
                        D3D12_DSV_DIMENSION_TEXTURE2DMS;
                }
            }
            else if (resource.DepthOrArraySize > 1) {
                description.ViewDimension =
                    D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                description.Texture2DArray.MipSlice = mip_slice;
                description.Texture2DArray.FirstArraySlice = first_slice;
                description.Texture2DArray.ArraySize = slice_count;
            }
            else {
                description.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                description.Texture2D.MipSlice = mip_slice;
            }
            break;

        default:
            break;
        }

        device->CreateDepthStencilView(resource_.Get(), &description, location);
    }

} // namespace fjr::dx
