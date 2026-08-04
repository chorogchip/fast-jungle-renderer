#include "FastJungle/dx12/Buffer.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

#include <cstdlib>
#include <limits>
#include <utility>

namespace fjr::dx {

    void Buffer::init(
        ID3D12Device* device,
        UINT64 size,
        D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES initial_state) {

        D3D12_HEAP_PROPERTIES heap_properties{};
        heap_properties.Type = heap_type;
        heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heap_properties.CreationNodeMask = 1;
        heap_properties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        description.Alignment = 0;
        description.Width = size;
        description.Height = 1;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.SampleDesc.Count = 1;
        description.SampleDesc.Quality = 0;
        description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        description.Flags = flags;

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;

        abort_failed(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &description,
            initial_state,
            nullptr,
            IID_PPV_ARGS(
                resource.ReleaseAndGetAddressOf())));

        set_resource(std::move(resource), initial_state);
    }

    void Buffer::create_typed_srv(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        DXGI_FORMAT format,
        UINT64 element_offset,
        UINT count) const {

        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format = format;
        description.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        description.Buffer.FirstElement = element_offset;
        description.Buffer.NumElements = count;
        description.Buffer.StructureByteStride = 0;
        description.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        device->CreateShaderResourceView(resource_.Get(), &description, location);
    }

    void Buffer::create_structured_srv(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        UINT structure_stride,
        UINT64 element_offset,
        UINT count) const {

        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        description.Buffer.FirstElement = element_offset;
        description.Buffer.NumElements = count;
        description.Buffer.StructureByteStride = structure_stride;
        description.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        device->CreateShaderResourceView(resource_.Get(), &description, location);
    }

    void Buffer::create_raw_srv(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        UINT64 element_offset,
        UINT count) const {

        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format = DXGI_FORMAT_R32_TYPELESS;
        description.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        description.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        description.Buffer.FirstElement = element_offset;
        description.Buffer.NumElements = count;
        description.Buffer.StructureByteStride = 0;
        description.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

        device->CreateShaderResourceView(resource_.Get(), &description, location);
    }

    void Buffer::create_typed_uav(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        DXGI_FORMAT format,
        UINT64 element_offset,
        UINT count) const {

        D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
        description.Format = format;
        description.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

        description.Buffer.FirstElement = element_offset;
        description.Buffer.NumElements = count;
        description.Buffer.StructureByteStride = 0;
        description.Buffer.CounterOffsetInBytes = 0;
        description.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        device->CreateUnorderedAccessView(resource_.Get(), nullptr, &description, location);
    }

    void Buffer::create_structured_uav(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        UINT structure_stride,
        UINT64 element_offset,
        UINT count,
        ID3D12Resource* counter_resource,
        UINT64 counter_offset) const {

        D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

        description.Buffer.FirstElement = element_offset;
        description.Buffer.NumElements = count;
        description.Buffer.StructureByteStride = structure_stride;
        description.Buffer.CounterOffsetInBytes = counter_offset;
        description.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        device->CreateUnorderedAccessView(
            resource_.Get(),
            counter_resource,
            &description,
            location);
    }

    void Buffer::create_raw_uav(
        ID3D12Device* device,
        D3D12_CPU_DESCRIPTOR_HANDLE location,
        UINT64 element_offset,
        UINT count) const {

        D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
        description.Format = DXGI_FORMAT_R32_TYPELESS;
        description.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

        description.Buffer.FirstElement = element_offset;
        description.Buffer.NumElements = count;
        description.Buffer.StructureByteStride = 0;
        description.Buffer.CounterOffsetInBytes = 0;
        description.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

        device->CreateUnorderedAccessView(resource_.Get(), nullptr, &description, location);
    }

} // namespace fjr::dx