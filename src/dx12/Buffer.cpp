#include "FastJungle/dx12/Buffer.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

#include <cstdlib>
#include <limits>
#include <utility>

namespace fjr::dx {

    namespace {

        UINT get_format_size(
            DXGI_FORMAT format) {

            switch (format) {
            case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            case DXGI_FORMAT_R32G32B32A32_FLOAT:
            case DXGI_FORMAT_R32G32B32A32_UINT:
            case DXGI_FORMAT_R32G32B32A32_SINT:
                return 16;

            case DXGI_FORMAT_R32G32B32_TYPELESS:
            case DXGI_FORMAT_R32G32B32_FLOAT:
            case DXGI_FORMAT_R32G32B32_UINT:
            case DXGI_FORMAT_R32G32B32_SINT:
                return 12;

            case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            case DXGI_FORMAT_R16G16B16A16_FLOAT:
            case DXGI_FORMAT_R16G16B16A16_UNORM:
            case DXGI_FORMAT_R16G16B16A16_UINT:
            case DXGI_FORMAT_R16G16B16A16_SNORM:
            case DXGI_FORMAT_R16G16B16A16_SINT:
            case DXGI_FORMAT_R32G32_TYPELESS:
            case DXGI_FORMAT_R32G32_FLOAT:
            case DXGI_FORMAT_R32G32_UINT:
            case DXGI_FORMAT_R32G32_SINT:
                return 8;

            case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            case DXGI_FORMAT_R10G10B10A2_UNORM:
            case DXGI_FORMAT_R10G10B10A2_UINT:
            case DXGI_FORMAT_R11G11B10_FLOAT:
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            case DXGI_FORMAT_R8G8B8A8_UINT:
            case DXGI_FORMAT_R8G8B8A8_SNORM:
            case DXGI_FORMAT_R8G8B8A8_SINT:
            case DXGI_FORMAT_R16G16_TYPELESS:
            case DXGI_FORMAT_R16G16_FLOAT:
            case DXGI_FORMAT_R16G16_UNORM:
            case DXGI_FORMAT_R16G16_UINT:
            case DXGI_FORMAT_R16G16_SNORM:
            case DXGI_FORMAT_R16G16_SINT:
            case DXGI_FORMAT_R32_TYPELESS:
            case DXGI_FORMAT_R32_FLOAT:
            case DXGI_FORMAT_R32_UINT:
            case DXGI_FORMAT_R32_SINT:
            case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
                return 4;

            case DXGI_FORMAT_R8G8_TYPELESS:
            case DXGI_FORMAT_R8G8_UNORM:
            case DXGI_FORMAT_R8G8_UINT:
            case DXGI_FORMAT_R8G8_SNORM:
            case DXGI_FORMAT_R8G8_SINT:
            case DXGI_FORMAT_R16_TYPELESS:
            case DXGI_FORMAT_R16_FLOAT:
            case DXGI_FORMAT_R16_UNORM:
            case DXGI_FORMAT_R16_UINT:
            case DXGI_FORMAT_R16_SNORM:
            case DXGI_FORMAT_R16_SINT:
                return 2;

            case DXGI_FORMAT_R8_TYPELESS:
            case DXGI_FORMAT_R8_UNORM:
            case DXGI_FORMAT_R8_UINT:
            case DXGI_FORMAT_R8_SNORM:
            case DXGI_FORMAT_R8_SINT:
            case DXGI_FORMAT_A8_UNORM:
                return 1;

            default:
                std::abort();
            }
        }

        UINT resolve_element_count(
            UINT64 buffer_size,
            UINT64 first_element,
            UINT element_count,
            UINT element_size) {

            if (element_size == 0) {
                std::abort();
            }

            const UINT64 total_element_count =
                buffer_size / element_size;

            if (first_element > total_element_count) {
                std::abort();
            }

            const UINT64 remaining =
                total_element_count - first_element;

            if (element_count != 0) {
                if (element_count > remaining) {
                    std::abort();
                }

                return element_count;
            }

            if (remaining >
                std::numeric_limits<UINT>::max()) {
                std::abort();
            }

            return static_cast<UINT>(remaining);
        }

    } // namespace

    void Buffer::init(
        ID3D12Device* device,
        UINT64 size,
        D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES initial_state) {

        D3D12_HEAP_PROPERTIES heap_properties{};
        heap_properties.Type = heap_type;
        heap_properties.CPUPageProperty =
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_properties.MemoryPoolPreference =
            D3D12_MEMORY_POOL_UNKNOWN;
        heap_properties.CreationNodeMask = 1;
        heap_properties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC description{};
        description.Dimension =
            D3D12_RESOURCE_DIMENSION_BUFFER;
        description.Alignment = 0;
        description.Width = size;
        description.Height = 1;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.SampleDesc.Count = 1;
        description.SampleDesc.Quality = 0;
        description.Layout =
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
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

        set_resource(
            std::move(resource),
            initial_state);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE
        Buffer::create_typed_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            DXGI_FORMAT format,
            UINT64 first_element,
            UINT element_count,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count()) {
            std::abort();
        }

        const UINT format_size =
            get_format_size(format);

        const UINT resolved_count =
            resolve_element_count(
                resource_->GetDesc().Width,
                first_element,
                element_count,
                format_size);

        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format = format;
        description.ViewDimension =
            D3D12_SRV_DIMENSION_BUFFER;
        description.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        description.Buffer.FirstElement =
            first_element;
        description.Buffer.NumElements =
            resolved_count;
        description.Buffer.StructureByteStride = 0;
        description.Buffer.Flags =
            D3D12_BUFFER_SRV_FLAG_NONE;

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
        Buffer::create_structured_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT structure_stride,
            UINT64 first_element,
            UINT element_count,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count() ||
            structure_stride == 0) {
            std::abort();
        }

        const UINT resolved_count =
            resolve_element_count(
                resource_->GetDesc().Width,
                first_element,
                element_count,
                structure_stride);

        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.ViewDimension =
            D3D12_SRV_DIMENSION_BUFFER;
        description.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        description.Buffer.FirstElement =
            first_element;
        description.Buffer.NumElements =
            resolved_count;
        description.Buffer.StructureByteStride =
            structure_stride;
        description.Buffer.Flags =
            D3D12_BUFFER_SRV_FLAG_NONE;

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
        Buffer::create_raw_srv(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT64 first_byte,
            UINT64 byte_size,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count() ||
            first_byte % 4 != 0 ||
            byte_size % 4 != 0) {
            std::abort();
        }

        const UINT64 buffer_size =
            resource_->GetDesc().Width;

        if (first_byte > buffer_size) {
            std::abort();
        }

        const UINT64 resolved_byte_size =
            byte_size == 0
            ? buffer_size - first_byte
            : byte_size;

        if (first_byte + resolved_byte_size >
            buffer_size ||
            resolved_byte_size % 4 != 0 ||
            resolved_byte_size / 4 >
            std::numeric_limits<UINT>::max()) {
            std::abort();
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC description{};
        description.Format =
            DXGI_FORMAT_R32_TYPELESS;
        description.ViewDimension =
            D3D12_SRV_DIMENSION_BUFFER;
        description.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        description.Buffer.FirstElement =
            first_byte / 4;
        description.Buffer.NumElements =
            static_cast<UINT>(
                resolved_byte_size / 4);
        description.Buffer.StructureByteStride = 0;
        description.Buffer.Flags =
            D3D12_BUFFER_SRV_FLAG_RAW;

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
        Buffer::create_typed_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            DXGI_FORMAT format,
            UINT64 first_element,
            UINT element_count,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count()) {
            std::abort();
        }

        const UINT format_size =
            get_format_size(format);

        const UINT resolved_count =
            resolve_element_count(
                resource_->GetDesc().Width,
                first_element,
                element_count,
                format_size);

        D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
        description.Format = format;
        description.ViewDimension =
            D3D12_UAV_DIMENSION_BUFFER;

        description.Buffer.FirstElement =
            first_element;
        description.Buffer.NumElements =
            resolved_count;
        description.Buffer.StructureByteStride = 0;
        description.Buffer.CounterOffsetInBytes = 0;
        description.Buffer.Flags =
            D3D12_BUFFER_UAV_FLAG_NONE;

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

    D3D12_GPU_DESCRIPTOR_HANDLE
        Buffer::create_structured_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT structure_stride,
            UINT64 first_element,
            UINT element_count,
            ID3D12Resource* counter_resource,
            UINT64 counter_offset,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count() ||
            structure_stride == 0) {
            std::abort();
        }

        const UINT resolved_count =
            resolve_element_count(
                resource_->GetDesc().Width,
                first_element,
                element_count,
                structure_stride);

        D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.ViewDimension =
            D3D12_UAV_DIMENSION_BUFFER;

        description.Buffer.FirstElement =
            first_element;
        description.Buffer.NumElements =
            resolved_count;
        description.Buffer.StructureByteStride =
            structure_stride;
        description.Buffer.CounterOffsetInBytes =
            counter_offset;
        description.Buffer.Flags =
            D3D12_BUFFER_UAV_FLAG_NONE;

        device->CreateUnorderedAccessView(
            resource_.Get(),
            counter_resource,
            &description,
            heap.get_cpu_handle(
                allocation,
                descriptor_offset));

        return heap.get_gpu_handle(
            allocation,
            descriptor_offset);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE
        Buffer::create_raw_uav(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            UINT64 first_byte,
            UINT64 byte_size,
            UINT descriptor_offset) const {

        if (descriptor_offset >= allocation.get_count() ||
            first_byte % 4 != 0 ||
            byte_size % 4 != 0) {
            std::abort();
        }

        const UINT64 buffer_size =
            resource_->GetDesc().Width;

        if (first_byte > buffer_size) {
            std::abort();
        }

        const UINT64 resolved_byte_size =
            byte_size == 0
            ? buffer_size - first_byte
            : byte_size;

        if (first_byte + resolved_byte_size >
            buffer_size ||
            resolved_byte_size % 4 != 0 ||
            resolved_byte_size / 4 >
            std::numeric_limits<UINT>::max()) {
            std::abort();
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC description{};
        description.Format =
            DXGI_FORMAT_R32_TYPELESS;
        description.ViewDimension =
            D3D12_UAV_DIMENSION_BUFFER;

        description.Buffer.FirstElement =
            first_byte / 4;
        description.Buffer.NumElements =
            static_cast<UINT>(
                resolved_byte_size / 4);
        description.Buffer.StructureByteStride = 0;
        description.Buffer.CounterOffsetInBytes = 0;
        description.Buffer.Flags =
            D3D12_BUFFER_UAV_FLAG_RAW;

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

} // namespace fjr::dx