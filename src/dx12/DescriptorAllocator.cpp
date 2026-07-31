#include "FastJungle/dx12/DescriptorAllocator.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace fjr::dx {

    DescriptorAllocator::RegionProxy::~RegionProxy() {
        release();
    }

    DescriptorAllocator::RegionProxy::RegionProxy(
        RegionProxy&& other) noexcept
        : owner_(std::exchange(other.owner_, nullptr)),
        start_index_(std::exchange(other.start_index_, 0)),
        capacity_(std::exchange(other.capacity_, 0)),
        cursor_(std::exchange(other.cursor_, 0)) {}

    DescriptorAllocator::RegionProxy&
        DescriptorAllocator::RegionProxy::operator=(
            RegionProxy&& other) noexcept {

        if (this == &other) {
            return *this;
        }

        release();

        owner_ = std::exchange(other.owner_, nullptr);
        start_index_ =
            std::exchange(other.start_index_, 0);
        capacity_ =
            std::exchange(other.capacity_, 0);
        cursor_ =
            std::exchange(other.cursor_, 0);

        return *this;
    }

    DescriptorAllocation
        DescriptorAllocator::RegionProxy::allocate(
            UINT count) {

        if (owner_ == nullptr ||
            count == 0 ||
            count > capacity_ - cursor_) {
            std::abort();
        }

        const UINT index =
            start_index_ + cursor_;

        cursor_ += count;

        return DescriptorAllocation{
            index,
            count
        };
    }

    void DescriptorAllocator::RegionProxy::release() noexcept {
        if (owner_ == nullptr) {
            return;
        }

        owner_->release_region(
            start_index_,
            capacity_);

        owner_ = nullptr;
        start_index_ = 0;
        capacity_ = 0;
        cursor_ = 0;
    }

    void DescriptorAllocator::init(
        UINT capacity) {

        capacity_ = capacity;

        free_regions_.clear();

        if (capacity_ != 0) {
            free_regions_.push_back({
                .start_index = 0,
                .capacity = capacity_
                });
        }
    }

    DescriptorAllocator::RegionProxy
        DescriptorAllocator::allocate_region(
            UINT capacity) {

        if (capacity == 0) {
            std::abort();
        }

        const auto iterator = std::find_if(
            free_regions_.begin(),
            free_regions_.end(),
            [capacity](const FreeRegion& region) {
                return region.capacity >= capacity;
            });

        if (iterator == free_regions_.end()) {
            std::abort();
        }

        const UINT start_index =
            iterator->start_index;

        if (iterator->capacity == capacity) {
            free_regions_.erase(iterator);
        } else {
            iterator->start_index += capacity;
            iterator->capacity -= capacity;
        }

        return RegionProxy{
            this,
            start_index,
            capacity
        };
    }

    void DescriptorAllocator::release_region(
        UINT start_index,
        UINT capacity) noexcept {

        if (capacity == 0) {
            return;
        }

        const auto iterator = std::lower_bound(
            free_regions_.begin(),
            free_regions_.end(),
            start_index,
            [](const FreeRegion& region, UINT index) {
                return region.start_index < index;
            });

        free_regions_.insert(
            iterator,
            FreeRegion{
                .start_index = start_index,
                .capacity = capacity
            });

        if (free_regions_.size() < 2) {
            return;
        }

        std::vector<FreeRegion> merged;
        merged.reserve(free_regions_.size());

        for (const FreeRegion& region : free_regions_) {
            if (merged.empty()) {
                merged.push_back(region);
                continue;
            }

            FreeRegion& previous = merged.back();

            const UINT previous_end =
                previous.start_index +
                previous.capacity;

            if (previous_end == region.start_index) {
                previous.capacity += region.capacity;
            } else {
                merged.push_back(region);
            }
        }

        free_regions_ = std::move(merged);
    }

} // namespace fjr::dx