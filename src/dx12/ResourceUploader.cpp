#include "FastJungle/dx12/ResourceUploader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::dx {

    void ResourceUploader::init(
        ID3D12Device* device,
        CommandQueue& command_queue,
        std::size_t page_size,
        std::size_t page_count) {

        command_queue_ = &command_queue;
        page_size_ = static_cast<UINT64>(page_size);
        contexts_.resize(page_count);
        upload_buffers_.resize(page_count);
        mapped_data_.resize(page_count);
        cursors_.assign(page_count, 0);

        for (std::size_t index = 0; index < page_count; ++index) {
            contexts_[index].init(
                device,
                command_queue_->get_type(),
                static_cast<UINT32>(index));
            upload_buffers_[index].init(
                device,
                page_size_,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            void* mapped = nullptr;
            const D3D12_RANGE read_range{0, 0};
            abort_failed(upload_buffers_[index]->Map(
                0,
                &read_range,
                &mapped));
            mapped_data_[index] = static_cast<std::byte*>(mapped);
        }
    }

    void ResourceUploader::upload_buffer(
        Buffer& destination,
        std::span<const std::byte> source,
        D3D12_RESOURCE_STATES final_state) {

        if (source.empty()) {
            return;
        }

        const UINT64 byte_size = static_cast<UINT64>(source.size());
        UINT64 source_offset = 0;
        while (source_offset < byte_size) {
            if (!recording_) {
                auto& context = contexts_[current_page_];
                command_queue_->wait(context.get_fence_value());
                context.reset();
                cursors_[current_page_] = 0;
                recording_ = true;
            }

            const UINT64 remaining = page_size_ - cursors_[current_page_];
            if (remaining == 0) {
                flush();
                continue;
            }

            const UINT64 copy_size = std::min(
                byte_size - source_offset,
                remaining);
            std::memcpy(
                mapped_data_[current_page_] + cursors_[current_page_],
                source.data() + static_cast<std::size_t>(source_offset),
                static_cast<std::size_t>(copy_size));
            contexts_[current_page_]->CopyBufferRegion(
                destination.get(),
                source_offset,
                upload_buffers_[current_page_].get(),
                cursors_[current_page_],
                copy_size);
            cursors_[current_page_] += copy_size;
            source_offset += copy_size;
        }

        destination.transition(
            contexts_[current_page_].get(),
            final_state);
    }

    void ResourceUploader::upload_buffer_gathered(
        Buffer& destination,
        std::span<const std::byte> source,
        std::size_t element_size,
        std::span<const std::uint32_t> source_order,
        D3D12_RESOURCE_STATES final_state) {

        if (source_order.empty()) {
            return;
        }

        std::size_t destination_index = 0;
        while (destination_index < source_order.size()) {
            if (!recording_) {
                auto& context = contexts_[current_page_];
                command_queue_->wait(context.get_fence_value());
                context.reset();
                cursors_[current_page_] = 0;
                recording_ = true;
            }

            const UINT64 remaining = page_size_ - cursors_[current_page_];
            const std::size_t element_count = std::min(
                source_order.size() - destination_index,
                static_cast<std::size_t>(remaining / element_size));
            if (element_count == 0) {
                flush();
                continue;
            }

            const UINT64 copy_size = static_cast<UINT64>(element_count) *
                element_size;
            auto* destination_data =
                mapped_data_[current_page_] + cursors_[current_page_];
            for (std::size_t index = 0; index < element_count; ++index) {
                std::memcpy(
                    destination_data + index * element_size,
                    source.data() + static_cast<std::size_t>(
                        source_order[destination_index + index]) *
                        element_size,
                    element_size);
            }

            contexts_[current_page_]->CopyBufferRegion(
                destination.get(),
                static_cast<UINT64>(destination_index) * element_size,
                upload_buffers_[current_page_].get(),
                cursors_[current_page_],
                copy_size);
            cursors_[current_page_] += copy_size;
            destination_index += element_count;
        }

        destination.transition(
            contexts_[current_page_].get(),
            final_state);
    }

    void ResourceUploader::upload_texture(
        Texture& destination,
        const TextureUploadDesc& source,
        D3D12_RESOURCE_STATES final_state) {

        if (source.subresources.empty()) {
            return;
        }

        UINT64 upload_offset = 0;
        for (;;) {
            if (!recording_) {
                auto& context = contexts_[current_page_];
                command_queue_->wait(context.get_fence_value());
                context.reset();
                cursors_[current_page_] = 0;
                recording_ = true;
            }

            const UINT64 remainder =
                cursors_[current_page_] %
                D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
            upload_offset = remainder == 0
                ? cursors_[current_page_]
                : cursors_[current_page_] +
                    D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - remainder;
            if (upload_offset <= page_size_ &&
                source.required_upload_size <= page_size_ - upload_offset) {
                break;
            }
            flush();
        }

        for (UINT index = 0;
             index < static_cast<UINT>(source.subresources.size());
             ++index) {
            const auto& source_data = source.subresources[index];
            auto footprint = source_data.base_footprint;
            footprint.Offset += upload_offset;
            auto* destination_data =
                mapped_data_[current_page_] + footprint.Offset;
            const UINT64 destination_slice_pitch =
                static_cast<UINT64>(footprint.Footprint.RowPitch) *
                source_data.row_count;

            for (UINT depth = 0;
                 depth < footprint.Footprint.Depth;
                 ++depth) {
                for (UINT row = 0; row < source_data.row_count; ++row) {
                    std::memcpy(
                        destination_data +
                            static_cast<std::size_t>(depth) *
                                destination_slice_pitch +
                            static_cast<std::size_t>(row) *
                                footprint.Footprint.RowPitch,
                        source_data.source +
                            static_cast<std::size_t>(depth) *
                                source_data.source_slice_pitch +
                            static_cast<std::size_t>(row) *
                                source_data.source_row_pitch,
                        static_cast<std::size_t>(source_data.row_size));
                }
            }

            D3D12_TEXTURE_COPY_LOCATION destination_location{};
            destination_location.pResource = destination.get();
            destination_location.Type =
                D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination_location.SubresourceIndex = index;

            D3D12_TEXTURE_COPY_LOCATION source_location{};
            source_location.pResource = upload_buffers_[current_page_].get();
            source_location.Type =
                D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source_location.PlacedFootprint = footprint;

            contexts_[current_page_]->CopyTextureRegion(
                &destination_location,
                0,
                0,
                0,
                &source_location,
                nullptr);
        }

        cursors_[current_page_] =
            upload_offset + source.required_upload_size;
        destination.transition(
            contexts_[current_page_].get(),
            final_state);
    }

    void ResourceUploader::flush() {
        if (!recording_) {
            return;
        }

        auto& context = contexts_[current_page_];
        context.close();
        command_queue_->execute(context.get());
        context.set_fence_value(command_queue_->signal());

        recording_ = false;
        current_page_ = (current_page_ + 1) % contexts_.size();
    }

    void ResourceUploader::reset() {
        if (command_queue_ != nullptr) {
            for (std::size_t index = 0; index < contexts_.size(); ++index) {
                command_queue_->wait(contexts_[index].get_fence_value());
                if (upload_buffers_[index]) {
                    upload_buffers_[index]->Unmap(0, nullptr);
                    upload_buffers_[index].reset();
                }
                contexts_[index] = CommandContext{};
            }
        }

        command_queue_ = nullptr;
        contexts_.clear();
        upload_buffers_.clear();
        mapped_data_.clear();
        cursors_.clear();
        page_size_ = 0;
        current_page_ = 0;
        recording_ = false;
    }

} // namespace fjr::dx
