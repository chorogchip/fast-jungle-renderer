#pragma once

#include <d3d12.h>

namespace fjr::dx {


    class DescAlloc {
    public:
        DescAlloc() = default;

        DescAlloc(
            D3D12_CPU_DESCRIPTOR_HANDLE handle_cpu,
            D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu,
            UINT count)
            : handle_cpu_{ handle_cpu },
            handle_gpu_{ handle_gpu },
            count_{ count } {}

        [[nodiscard]]
        D3D12_CPU_DESCRIPTOR_HANDLE get_cpu() const noexcept {
            return handle_cpu_;
        }

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE get_gpu() const noexcept {
            return handle_gpu_;
        }

        [[nodiscard]]
        UINT get_count() const noexcept {
            return count_;
        }

    private:
        D3D12_CPU_DESCRIPTOR_HANDLE handle_cpu_{};
        D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu_{};
        UINT count_ = 0;
    };

    class CBbufArrayView {

    public:
        CBbufArrayView() = default;
        CBbufArrayView(D3D12_GPU_VIRTUAL_ADDRESS base, UINT stride, UINT size)
            :base_{ base }, stride_{ stride }, size_{ size } {}
        CBbufArrayView(const CBbufArrayView&) = default;
        CBbufArrayView& operator=(const CBbufArrayView&) = default;

        D3D12_GPU_VIRTUAL_ADDRESS at(UINT index) const {
            return base_ + static_cast<UINT64>(index) * stride_;
        }

    private:
        D3D12_GPU_VIRTUAL_ADDRESS base_;
        UINT stride_;
        UINT size_;
    };

}