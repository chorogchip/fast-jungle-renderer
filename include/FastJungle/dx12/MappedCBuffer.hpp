#pragma once

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

#include <d3d12.h>
#include <new>
#include <source_location>

namespace fjr::dx {

    template<typename T>
    class MappedCBuffer {
    public:
        void init(
            ID3D12Device* device,
            std::source_location loc =
                std::source_location::current()) {
            constexpr UINT64 alignment =
                D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
            constexpr UINT64 size =
                (sizeof(T) + alignment - 1) & ~(alignment - 1);

            buffer_.init(
                device,
                size,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                loc);

            void* mapped = nullptr;
            const D3D12_RANGE read_range{0, 0};
            abort_failed(buffer_->Map(0, &read_range, &mapped), loc);
            data_ = ::new (mapped) T{};
        }

        [[nodiscard]]
        T& data() noexcept {
            return *data_;
        }

        [[nodiscard]]
        D3D12_GPU_VIRTUAL_ADDRESS get_address() const noexcept {
            return buffer_->GetGPUVirtualAddress();
        }

    private:
        Buffer buffer_;
        T* data_ = nullptr;
    };

} // namespace fjr::dx
