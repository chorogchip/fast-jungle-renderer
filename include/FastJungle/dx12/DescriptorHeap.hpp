#pragma once

#include <wrl.h>
#include <d3d12.h>

#include "FastJungle/dx12/DescriptorAllocator.hpp"

namespace fjr::dx {

    class DescriptorHeap {
    public:
        DescriptorHeap() = default;

        DescriptorHeap(const DescriptorHeap&) = delete;
        DescriptorHeap& operator=(const DescriptorHeap&) = delete;

        DescriptorHeap(DescriptorHeap&&) noexcept = default;
        DescriptorHeap& operator=(DescriptorHeap&&) noexcept = default;

        void init(
            ID3D12Device* device,
            D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT capacity,
            bool shader_visible);

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE
            get_cpu_handle(UINT index) const noexcept {
            auto handle = cpu_start_;
            handle.ptr +=
                static_cast<SIZE_T>(index) * descriptor_size_;
            return handle;
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE
            get_gpu_handle(UINT index) const noexcept {
            auto handle = gpu_start_;
            handle.ptr +=
                static_cast<UINT64>(index) * descriptor_size_;
            return handle;
        }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(
                const DescriptorAllocation& allocation,
                UINT offset) const noexcept {

            if (offset >= allocation.get_count()) {
                return {};
            }

            return get_cpu_handle(
                allocation.get_index() + offset);
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(
                const DescriptorAllocation& allocation,
                UINT offset) const noexcept {

            if (offset >= allocation.get_count()) {
                return {};
            }

            return get_gpu_handle(
                allocation.get_index() + offset);
        }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE
            get_cpu_start() const noexcept {
            return cpu_start_;
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE
            get_gpu_start() const noexcept {
            return gpu_start_;
        }

        [[nodiscard]] ID3D12DescriptorHeap*
            get_descriptor_heap() const noexcept {
            return descriptor_heap_.Get();
        }

        [[nodiscard]] D3D12_DESCRIPTOR_HEAP_TYPE
            get_type() const noexcept {
            return type_;
        }

        [[nodiscard]] UINT get_descriptor_size() const noexcept {
            return descriptor_size_;
        }

        [[nodiscard]] UINT get_capacity() const noexcept {
            return capacity_;
        }

        [[nodiscard]] bool get_shader_visible() const noexcept {
            return shader_visible_;
        }

        [[nodiscard]] ID3D12DescriptorHeap*
            operator->() const noexcept {
            return descriptor_heap_.Get();
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return descriptor_heap_ != nullptr;
        }

    private:
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap_;

        D3D12_CPU_DESCRIPTOR_HANDLE cpu_start_{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_start_{};

        D3D12_DESCRIPTOR_HEAP_TYPE type_ =
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

        UINT descriptor_size_ = 0;
        UINT capacity_ = 0;

        bool shader_visible_ = false;
    };

} // namespace fjr::dx