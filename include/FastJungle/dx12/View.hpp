#pragma once

#include <d3d12.h>

namespace fjr::dx {

    class DescAlloc {
    public:
        DescAlloc() = default;

        DescAlloc(
            D3D12_CPU_DESCRIPTOR_HANDLE handle_cpu,
            D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu,
            UINT count,
            UINT descriptor_size)
            : handle_cpu_{ handle_cpu },
            handle_gpu_{ handle_gpu },
            count_{ count },
            descriptor_size_{ descriptor_size } {}

        [[nodiscard]]
        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu(UINT offset = 0) const noexcept {
            auto handle = handle_cpu_;
            handle.ptr += static_cast<SIZE_T>(offset) * descriptor_size_;
            return handle;
        }

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu(UINT offset = 0) const noexcept {
            auto handle = handle_gpu_;
            handle.ptr += static_cast<UINT64>(offset) * descriptor_size_;
            return handle;
        }

        [[nodiscard]]
        UINT get_count() const noexcept {
            return count_;
        }

    private:
        D3D12_CPU_DESCRIPTOR_HANDLE handle_cpu_{};
        D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu_{};
        UINT count_ = 0;
        UINT descriptor_size_ = 0;
    };

    class CBufferArrayView {
    public:
        CBufferArrayView() = default;

        CBufferArrayView(
            D3D12_GPU_VIRTUAL_ADDRESS base,
            UINT64 stride) noexcept
            : base_{ base }, stride_{ stride } {}

        [[nodiscard]]
        D3D12_GPU_VIRTUAL_ADDRESS get_address(UINT index) const noexcept {
            return base_ + static_cast<UINT64>(index) * stride_;
        }

    private:
        D3D12_GPU_VIRTUAL_ADDRESS base_ = 0;
        UINT64 stride_ = 0;
    };

} // namespace fjr::dx
