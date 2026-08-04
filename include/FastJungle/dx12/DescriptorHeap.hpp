#pragma once

#include <wrl.h>
#include <d3d12.h>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/View.hpp"

namespace fjr::dx {

    class DescriptorHeap {

    public:

        class DescArena {

        public:
            DescArena(const DescArena&) = delete;
            DescArena& operator=(const DescArena&) = delete;
            DescArena(DescArena&&) = default;
            DescArena& operator=(DescArena&&) = default;

            DescAlloc alloc(UINT count = 1) {
                log::Logger::g_logger << log::asrt(size_ + count <= capacity_);

                const UINT index = offset_ + size_;

                size_ += count;
                return DescAlloc{
                    heap_->get_cpu_handle(index),
                    heap_->get_gpu_handle(index),
                    count
                };
            }

            DescArena alloc_arena(UINT capacity) {
                log::Logger::g_logger << log::asrt(size_ + capacity <= capacity_);

                DescArena ret{};
                ret.offset_ = offset_ + size_;
                ret.capacity_ = capacity;
                ret.size_ = 0;
                ret.heap_ = heap_;

                size_ += capacity;
                return ret;
            }

        private:
            DescArena() = default;
            friend class DescriptorHeap;

            UINT offset_;
            UINT capacity_;
            UINT size_;
            DescriptorHeap* heap_;
        };

        DescriptorHeap() = default;

        void init(
            ID3D12Device* device,
            D3D12_DESCRIPTOR_HEAP_TYPE type,
            UINT capacity,
            bool shader_visible);

        void reset();

        DescArena alloc_arena(UINT capacity) {
            log::Logger::g_logger << log::asrt(size_ + capacity <= capacity_);

            DescArena ret{};
            ret.offset_ = size_;
            ret.capacity_ = capacity;
            ret.size_ = 0;
            ret.heap_ = this;

            size_ += capacity;
            return ret;
        }

        DescArena default_arena() {
            DescArena ret{};
            ret.offset_ = 0;
            ret.capacity_ = capacity_;
            ret.size_ = 0;
            ret.heap_ = this;
            return ret;
        }

        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(UINT index) const noexcept {
            auto handle = cpu_start_;
            handle.ptr += static_cast<SIZE_T>(index) * descriptor_size_;
            return handle;
        }

        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(UINT index) const noexcept {
            auto handle = gpu_start_;
            handle.ptr += static_cast<UINT64>(index) * descriptor_size_;
            return handle;
        }

        [[nodiscard]] ID3D12DescriptorHeap* get() const noexcept {
            return descriptor_heap_.Get();
        }

    private:
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptor_heap_;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_start_{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_start_{};
        UINT descriptor_size_ = 0;

        UINT capacity_ = 0;
        UINT size_ = 0;
    };

} // namespace fjr::dx