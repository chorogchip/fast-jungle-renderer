#pragma once

#include <d3d12.h>

#include <vector>

namespace fjr::dx {

    class DescriptorAllocation {
    public:
        DescriptorAllocation() = default;

        [[nodiscard]]
        UINT get_index() const noexcept {
            return index_;
        }

        [[nodiscard]]
        UINT get_count() const noexcept {
            return count_;
        }

        [[nodiscard]]
        explicit operator bool() const noexcept {
            return count_ != 0;
        }

    private:
        friend class DescriptorAllocator;

        DescriptorAllocation(
            UINT index,
            UINT count) noexcept
            : index_(index),
            count_(count) {}

        UINT index_ = 0;
        UINT count_ = 0;
    };

    class DescriptorAllocator {
    public:
        class RegionProxy {
        public:
            RegionProxy() = default;
            ~RegionProxy();

            RegionProxy(const RegionProxy&) = delete;
            RegionProxy& operator=(const RegionProxy&) = delete;

            RegionProxy(RegionProxy&& other) noexcept;
            RegionProxy& operator=(
                RegionProxy&& other) noexcept;

            [[nodiscard]]
            DescriptorAllocation allocate(
                UINT count = 1);

            void reset() noexcept {
                cursor_ = 0;
            }

            void release() noexcept;

            [[nodiscard]]
            UINT get_start_index() const noexcept {
                return start_index_;
            }

            [[nodiscard]]
            UINT get_capacity() const noexcept {
                return capacity_;
            }

            [[nodiscard]]
            UINT get_size() const noexcept {
                return cursor_;
            }

            [[nodiscard]]
            UINT get_remaining() const noexcept {
                return capacity_ - cursor_;
            }

            [[nodiscard]]
            explicit operator bool() const noexcept {
                return owner_ != nullptr;
            }

        private:
            friend class DescriptorAllocator;

            RegionProxy(
                DescriptorAllocator* owner,
                UINT start_index,
                UINT capacity) noexcept
                : owner_(owner),
                start_index_(start_index),
                capacity_(capacity) {}

            DescriptorAllocator* owner_ = nullptr;
            UINT start_index_ = 0;
            UINT capacity_ = 0;
            UINT cursor_ = 0;
        };

        DescriptorAllocator() = default;

        DescriptorAllocator(
            const DescriptorAllocator&) = delete;

        DescriptorAllocator& operator=(
            const DescriptorAllocator&) = delete;

        DescriptorAllocator(
            DescriptorAllocator&&) = delete;

        DescriptorAllocator& operator=(
            DescriptorAllocator&&) = delete;

        void init(UINT capacity);

        [[nodiscard]]
        RegionProxy allocate_region(UINT capacity);

        [[nodiscard]]
        UINT get_capacity() const noexcept {
            return capacity_;
        }

    private:
        struct FreeRegion {
            UINT start_index = 0;
            UINT capacity = 0;
        };

        void release_region(
            UINT start_index,
            UINT capacity) noexcept;

        UINT capacity_ = 0;
        std::vector<FreeRegion> free_regions_;
    };

} // namespace fjr::dx