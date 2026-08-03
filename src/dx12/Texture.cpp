#include "FastJungle/dx12/Texture.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace fjr::dx {

    namespace {

        UINT resolve_mip_count(
            const D3D12_RESOURCE_DESC& resource,
            UINT first_mip,
            UINT mip_count) {

            if (first_mip >= resource.MipLevels) {
                std::abort();
            }

            const UINT remaining =
                resource.MipLevels - first_mip;

            if (mip_count == 0) {
                return remaining;
            }

            if (mip_count > remaining) {
                std::abort();
            }

            return mip_count;
        }

        UINT resolve_slice_count(
            UINT total_slice_count,
            UINT first_slice,
            UINT slice_count) {

            if (first_slice >= total_slice_count) {
                std::abort();
            }

            const UINT remaining =
                total_slice_count - first_slice;

            if (slice_count == 0) {
                return remaining;
            }

            if (slice_count > remaining) {
                std::abort();
            }

            return slice_count;
        }

        UINT get_depth_at_mip(
            const D3D12_RESOURCE_DESC& resource,
            UINT mip_slice) {

            return std::max(
                1u,
                static_cast<UINT>(
                    resource.DepthOrArraySize) >>
                mip_slice);
        }

        DXGI_FORMAT resolve_format(
            const D3D12_RESOURCE_DESC& resource,
            DXGI_FORMAT format) {

            return format == DXGI_FORMAT_UNKNOWN
                ? resource.Format
                : format;
        }

    } // namespace

    void Texture::init(
        ID3D12Device* device,
        const D3D12_RESOURCE_DESC& description,
        TextureType type,
        D3D12_RESOURCE_STATES initial_state,
        const D3D12_CLEAR_VALUE* clear_value) {

        D3D12_HEAP_PROPERTIES heap_properties{};
        heap_properties.Type =
            D3D12_HEAP_TYPE_DEFAULT;
        heap_properties.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_properties.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        heap_properties.CreationNodeMask = 1;
        heap_properties.VisibleNodeMask = 1;

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;

        abort_failed(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &description,
            initial_state,
            clear_value,
            IID_PPV_ARGS(
                resource.ReleaseAndGetAddressOf())));

        this->set_resource(
            std::move(resource),
            initial_state);

        type_ = type;
    }

    void Texture::attach(
        ID3D12Resource* resource,
        TextureType type,
        D3D12_RESOURCE_STATES initial_state) {

        Microsoft::WRL::ComPtr<ID3D12Resource>
            resource_pointer = resource;

        this->set_resource(
            std::move(resource_pointer),
            initial_state);

        type_ = type;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE
        Texture::create_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            const TextureViewRange& range,
            DXGI_FORMAT format,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count()) {
            std::abort();
        }

        const D3D12_RESOURCE_DESC resource =
            resource_->GetDesc();

        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format =
            resolve_format(resource, format);
        description.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        switch (type_) {
        case TextureType::texture2d:
            if (resource.SampleDesc.Count > 1) {
                description.ViewDimension =
                    D3D12_SRV_DIMENSION_TEXTURE2DMS;
            } else {
                description.ViewDimension =
                    D3D12_SRV_DIMENSION_TEXTURE2D;

                description.Texture2D.MostDetailedMip =
                    range.first_mip;
                description.Texture2D.MipLevels =
                    resolve_mip_count(
                        resource,
                        range.first_mip,
                        range.mip_count);
                description.Texture2D.PlaneSlice = 0;
                description.Texture2D.ResourceMinLODClamp =
                    0.0f;
            }
            break;

        case TextureType::texture2d_array: {
            const UINT slice_count =
                resolve_slice_count(
                    resource.DepthOrArraySize,
                    range.first_slice,
                    range.slice_count);

            if (resource.SampleDesc.Count > 1) {
                description.ViewDimension =
                    D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;

                description.Texture2DMSArray.FirstArraySlice =
                    range.first_slice;
                description.Texture2DMSArray.ArraySize =
                    slice_count;
            } else {
                description.ViewDimension =
                    D3D12_SRV_DIMENSION_TEXTURE2DARRAY;

                description.Texture2DArray.MostDetailedMip =
                    range.first_mip;
                description.Texture2DArray.MipLevels =
                    resolve_mip_count(
                        resource,
                        range.first_mip,
                        range.mip_count);
                description.Texture2DArray.FirstArraySlice =
                    range.first_slice;
                description.Texture2DArray.ArraySize =
                    slice_count;
                description.Texture2DArray.PlaneSlice = 0;
                description.Texture2DArray.ResourceMinLODClamp =
                    0.0f;
            }
            break;
        }

        case TextureType::texture_cube:
            description.ViewDimension =
                D3D12_SRV_DIMENSION_TEXTURECUBE;

            description.TextureCube.MostDetailedMip =
                range.first_mip;
            description.TextureCube.MipLevels =
                resolve_mip_count(
                    resource,
                    range.first_mip,
                    range.mip_count);
            description.TextureCube.ResourceMinLODClamp =
                0.0f;
            break;

        case TextureType::texture_cube_array: {
            const UINT face_count =
                resolve_slice_count(
                    resource.DepthOrArraySize,
                    range.first_slice,
                    range.slice_count);

            if (range.first_slice % 6 != 0 ||
                face_count % 6 != 0) {
                std::abort();
            }

            description.ViewDimension =
                D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;

            description.TextureCubeArray.MostDetailedMip =
                range.first_mip;
            description.TextureCubeArray.MipLevels =
                resolve_mip_count(
                    resource,
                    range.first_mip,
                    range.mip_count);
            description.TextureCubeArray.First2DArrayFace =
                range.first_slice;
            description.TextureCubeArray.NumCubes =
                face_count / 6;
            description.TextureCubeArray.ResourceMinLODClamp =
                0.0f;
            break;
        }

        case TextureType::texture3d:
            description.ViewDimension =
                D3D12_SRV_DIMENSION_TEXTURE3D;

            description.Texture3D.MostDetailedMip =
                range.first_mip;
            description.Texture3D.MipLevels =
                resolve_mip_count(
                    resource,
                    range.first_mip,
                    range.mip_count);
            description.Texture3D.ResourceMinLODClamp =
                0.0f;
            break;

        default:
            std::abort();
        }

        device->CreateShaderResourceView(
            resource_.Get(),
            &description,
            heap.get_cpu_handle(
                allocation,
                descriptor_offset));

        return heap.get_gpu_handle(
            allocation,
            descriptor_offset);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE
        Texture::create_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT mip_slice,
            UINT first_slice,
            UINT slice_count,
            DXGI_FORMAT format,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count()) {
            std::abort();
        }

        const D3D12_RESOURCE_DESC resource =
            resource_->GetDesc();

        if (resource.SampleDesc.Count > 1 ||
            mip_slice >= resource.MipLevels) {
            std::abort();
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
        description.Format =
            resolve_format(resource, format);

        switch (type_) {
        case TextureType::texture2d:
            description.ViewDimension =
                D3D12_UAV_DIMENSION_TEXTURE2D;

            description.Texture2D.MipSlice =
                mip_slice;
            description.Texture2D.PlaneSlice = 0;
            break;

        case TextureType::texture2d_array:
        case TextureType::texture_cube:
        case TextureType::texture_cube_array:
            description.ViewDimension =
                D3D12_UAV_DIMENSION_TEXTURE2DARRAY;

            description.Texture2DArray.MipSlice =
                mip_slice;
            description.Texture2DArray.FirstArraySlice =
                first_slice;
            description.Texture2DArray.ArraySize =
                resolve_slice_count(
                    resource.DepthOrArraySize,
                    first_slice,
                    slice_count);
            description.Texture2DArray.PlaneSlice = 0;
            break;

        case TextureType::texture3d: {
            const UINT depth =
                get_depth_at_mip(
                    resource,
                    mip_slice);

            description.ViewDimension =
                D3D12_UAV_DIMENSION_TEXTURE3D;

            description.Texture3D.MipSlice =
                mip_slice;
            description.Texture3D.FirstWSlice =
                first_slice;
            description.Texture3D.WSize =
                resolve_slice_count(
                    depth,
                    first_slice,
                    slice_count);
            break;
        }

        default:
            std::abort();
        }

        device->CreateUnorderedAccessView(
            resource_.Get(),
            nullptr,
            &description,
            heap.get_cpu_handle(
                allocation,
                descriptor_offset));

        return heap.get_gpu_handle(
            allocation,
            descriptor_offset);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE
        Texture::create_rtv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT mip_slice,
            UINT first_slice,
            UINT slice_count,
            DXGI_FORMAT format,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count()) {
            std::abort();
        }

        const D3D12_RESOURCE_DESC resource =
            resource_->GetDesc();

        if (mip_slice >= resource.MipLevels) {
            std::abort();
        }

        D3D12_RENDER_TARGET_VIEW_DESC description{};
        description.Format =
            resolve_format(resource, format);

        switch (type_) {
        case TextureType::texture2d:
            if (resource.SampleDesc.Count > 1) {
                description.ViewDimension =
                    D3D12_RTV_DIMENSION_TEXTURE2DMS;
            } else {
                description.ViewDimension =
                    D3D12_RTV_DIMENSION_TEXTURE2D;

                description.Texture2D.MipSlice =
                    mip_slice;
                description.Texture2D.PlaneSlice = 0;
            }
            break;

        case TextureType::texture2d_array:
        case TextureType::texture_cube:
        case TextureType::texture_cube_array: {
            const UINT resolved_slice_count =
                resolve_slice_count(
                    resource.DepthOrArraySize,
                    first_slice,
                    slice_count);

            if (resource.SampleDesc.Count > 1) {
                description.ViewDimension =
                    D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;

                description.Texture2DMSArray.FirstArraySlice =
                    first_slice;
                description.Texture2DMSArray.ArraySize =
                    resolved_slice_count;
            } else {
                description.ViewDimension =
                    D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

                description.Texture2DArray.MipSlice =
                    mip_slice;
                description.Texture2DArray.FirstArraySlice =
                    first_slice;
                description.Texture2DArray.ArraySize =
                    resolved_slice_count;
                description.Texture2DArray.PlaneSlice = 0;
            }
            break;
        }

        case TextureType::texture3d: {
            const UINT depth =
                get_depth_at_mip(
                    resource,
                    mip_slice);

            description.ViewDimension =
                D3D12_RTV_DIMENSION_TEXTURE3D;

            description.Texture3D.MipSlice =
                mip_slice;
            description.Texture3D.FirstWSlice =
                first_slice;
            description.Texture3D.WSize =
                resolve_slice_count(
                    depth,
                    first_slice,
                    slice_count);
            break;
        }

        default:
            std::abort();
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE handle =
            heap.get_cpu_handle(
                allocation,
                descriptor_offset);

        device->CreateRenderTargetView(
            resource_.Get(),
            &description,
            handle);

        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE
        Texture::create_dsv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT mip_slice,
            UINT first_slice,
            UINT slice_count,
            DXGI_FORMAT format,
            D3D12_DSV_FLAGS flags,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count()) {
            std::abort();
        }

        const D3D12_RESOURCE_DESC resource =
            resource_->GetDesc();

        if (mip_slice >= resource.MipLevels ||
            type_ == TextureType::texture3d) {
            std::abort();
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC description{};
        description.Format =
            resolve_format(resource, format);
        description.Flags = flags;

        switch (type_) {
        case TextureType::texture2d:
            if (resource.SampleDesc.Count > 1) {
                description.ViewDimension =
                    D3D12_DSV_DIMENSION_TEXTURE2DMS;
            } else {
                description.ViewDimension =
                    D3D12_DSV_DIMENSION_TEXTURE2D;

                description.Texture2D.MipSlice =
                    mip_slice;
            }
            break;

        case TextureType::texture2d_array:
        case TextureType::texture_cube:
        case TextureType::texture_cube_array: {
            const UINT resolved_slice_count =
                resolve_slice_count(
                    resource.DepthOrArraySize,
                    first_slice,
                    slice_count);

            if (resource.SampleDesc.Count > 1) {
                description.ViewDimension =
                    D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;

                description.Texture2DMSArray.FirstArraySlice =
                    first_slice;
                description.Texture2DMSArray.ArraySize =
                    resolved_slice_count;
            } else {
                description.ViewDimension =
                    D3D12_DSV_DIMENSION_TEXTURE2DARRAY;

                description.Texture2DArray.MipSlice =
                    mip_slice;
                description.Texture2DArray.FirstArraySlice =
                    first_slice;
                description.Texture2DArray.ArraySize =
                    resolved_slice_count;
            }
            break;
        }

        default:
            std::abort();
        }

        const D3D12_CPU_DESCRIPTOR_HANDLE handle =
            heap.get_cpu_handle(
                allocation,
                descriptor_offset);

        device->CreateDepthStencilView(
            resource_.Get(),
            &description,
            handle);

        return handle;
    }

} // namespace fjr::dx