#include "FastJungle/dx12/ResourceUploader.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::dx {

    ResourceUploader::~ResourceUploader() {
        reset();
    }

    void ResourceUploader::init(
        ID3D12Device* device,
        CommandQueue& command_queue,
        std::size_t page_size,
        std::size_t page_count) {

        if (device == nullptr || page_size == 0 || page_count == 0) {
            log::Logger::g_logger << log::abrt(
                "ResourceUploader requires a device and non-zero pages.");
        }

        this->reset();

        command_queue_ = &command_queue;
        page_size_ = static_cast<UINT64>(page_size);
        current_page_ = 0;
        recording_ = false;

        contexts_.resize(page_count);
        upload_buffers_.resize(page_count);
        mapped_data_.resize(page_count, nullptr);
        cursors_.resize(page_count, 0);

        for (std::size_t index = 0; index < page_count; ++index) {
            auto& context = contexts_[index];
            auto& upload_buffer = upload_buffers_[index];

            context.init(device, command_queue.get_type(), static_cast<UINT32>(index));

            upload_buffer.init(
                device,
                page_size_,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            void* mapped = nullptr;
            const D3D12_RANGE read_range{ 0, 0 };

            abort_failed(upload_buffer->Map(0, &read_range, &mapped));
            mapped_data_[index] = static_cast<std::byte*>(mapped);
        }
    }

    void ResourceUploader::begin_recording() {
        if (recording_) return;

        auto& context = contexts_[current_page_];
        const UINT64 fence_value = context.get_fence_value();

        if (fence_value != 0) command_queue_->wait(fence_value);

        context.reset();
        cursors_[current_page_] = 0;
        recording_ = true;
    }

    UINT64 ResourceUploader::reserve_upload_space(UINT64 size, UINT64 alignment) {
        if (alignment == 0 || size > page_size_) {
            log::Logger::g_logger << log::abrt(
                "ResourceUploader reservation is invalid.");
        }

        int i = 0;
        do {
            this->begin_recording();

            const UINT64 remainder = cursors_[current_page_] % alignment;
            UINT64 offset = cursors_[current_page_];
            if (remainder != 0) offset += alignment - remainder;

            if (offset <= page_size_ && size <= page_size_ - offset)
                return offset;

            if (i++) break;

            this->flush();
        } while (true);

        log::Logger::g_logger << log::abrt();
        return -1;
    }

    void ResourceUploader::upload_buffer(
        Buffer& destination,
        std::span<const std::byte> source,
        D3D12_RESOURCE_STATES final_state) {

        if (source.empty()) return;

        this->begin_recording();

        if (cursors_[current_page_] == page_size_) {
            this->flush();
            this->begin_recording();
        }

        destination.transition(
            contexts_[current_page_].get(),
            D3D12_RESOURCE_STATE_COPY_DEST);

        UINT64 destination_offset = 0;
        const UINT64 byte_size = static_cast<UINT64>(source.size());

        while (destination_offset < byte_size) {
            this->begin_recording();

            const UINT64 available = page_size_ - cursors_[current_page_];

            if (available == 0) {
                this->flush();
                continue;
            }

            const UINT64 copy_size =
                std::min(byte_size - destination_offset, available);

            std::memcpy(
                mapped_data_[current_page_] + cursors_[current_page_],
                source.data() + static_cast<std::size_t>(destination_offset),
                static_cast<std::size_t>(copy_size));

            contexts_[current_page_]->CopyBufferRegion(
                destination.get(),
                destination_offset,
                upload_buffers_[current_page_].get(),
                cursors_[current_page_],
                copy_size);

            cursors_[current_page_] += copy_size;
            destination_offset += copy_size;
        }

        destination.transition(contexts_[current_page_].get(), final_state);
    }

    void ResourceUploader::upload_buffer_gathered(
        Buffer& destination,
        std::span<const std::byte> source,
        std::size_t element_size,
        std::span<const std::uint32_t> source_order,
        D3D12_RESOURCE_STATES final_state) {

        if (source_order.empty()) return;

        log::Logger::g_logger <<
            log::asrt(element_size != 0) <<
            log::asrt(source_order.size() <=
                std::numeric_limits<std::size_t>::max() / element_size) <<
            log::asrt(source.size() >= element_size);

        const std::size_t total_size = source_order.size() * element_size;
        const std::size_t source_element_count = source.size() / element_size;

        if (source.size() % element_size != 0 ||
            std::ranges::any_of(
                source_order,
                [source_element_count](std::uint32_t source_index) {
                    return source_index >= source_element_count;
                })) {
            log::Logger::g_logger << log::abrt(
                "Gathered upload source order is out of bounds.");
        }

        this->begin_recording();

        if (cursors_[current_page_] == page_size_) {
            this->flush();
            this->begin_recording();
        }

        destination.transition(
            contexts_[current_page_].get(),
            D3D12_RESOURCE_STATE_COPY_DEST);

        std::size_t destination_offset = 0;

        while (destination_offset < total_size) {
            this->begin_recording();

            const UINT64 available = page_size_ - cursors_[current_page_];

            if (available == 0) {
                this->flush();
                continue;
            }

            const std::size_t remaining = total_size - destination_offset;

            const std::size_t copy_size =
                static_cast<std::size_t>(
                    std::min<UINT64>(
                        static_cast<UINT64>(remaining),
                        available));

            auto* upload_destination =
                mapped_data_[current_page_] + cursors_[current_page_];

            std::size_t packed_size = 0;

            while (packed_size < copy_size) {
                const std::size_t global_offset =
                    destination_offset + packed_size;

                const std::size_t destination_element =
                    global_offset / element_size;

                const std::size_t element_offset =
                    global_offset % element_size;

                const std::size_t source_element =
                    static_cast<std::size_t>(
                        source_order[destination_element]);

                const std::size_t fragment_size =
                    std::min(
                        copy_size - packed_size,
                        element_size - element_offset);

                std::memcpy(
                    upload_destination + packed_size,
                    source.data() +
                    source_element * element_size +
                    element_offset,
                    fragment_size);

                packed_size += fragment_size;
            }

            contexts_[current_page_]->CopyBufferRegion(
                destination.get(),
                static_cast<UINT64>(destination_offset),
                upload_buffers_[current_page_].get(),
                cursors_[current_page_],
                static_cast<UINT64>(copy_size));

            cursors_[current_page_] += static_cast<UINT64>(copy_size);
            destination_offset += copy_size;
        }

        destination.transition(contexts_[current_page_].get(), final_state);
    }

    void ResourceUploader::upload_texture(
        Texture& destination,
        const TextureUploadDesc& source,
        D3D12_RESOURCE_STATES final_state) {

        if (source.subresources.empty()) return;
        if (source.subresources.size() > std::numeric_limits<UINT>::max() ||
            source.required_upload_size == 0) {
            log::Logger::g_logger << log::abrt(
                "Texture upload description is invalid.");
        }

        bool transitioned_to_copy_dest = false;

        for (UINT index = 0;
            index < static_cast<UINT>(source.subresources.size());
            ++index) {

            const auto& source_data = source.subresources[index];
            auto footprint = source_data.base_footprint;

            const bool upload_slice_pitch_valid =
                source_data.row_count == 0 ||
                footprint.Footprint.RowPitch <=
                    std::numeric_limits<UINT64>::max() /
                        source_data.row_count;
            const UINT64 upload_slice_pitch = upload_slice_pitch_valid
                ? static_cast<UINT64>(footprint.Footprint.RowPitch) *
                    source_data.row_count
                : 0;
            const bool upload_size_valid =
                footprint.Footprint.Depth == 0 ||
                upload_slice_pitch <=
                    std::numeric_limits<UINT64>::max() /
                        footprint.Footprint.Depth;
            const UINT64 subresource_upload_size = upload_size_valid
                ? upload_slice_pitch * footprint.Footprint.Depth
                : 0;

            const bool source_slice_pitch_valid =
                source_data.row_count == 0 ||
                source_data.source_row_pitch <=
                    std::numeric_limits<UINT64>::max() /
                        source_data.row_count;
            const UINT64 minimum_source_slice_pitch =
                source_slice_pitch_valid
                    ? source_data.source_row_pitch * source_data.row_count
                    : 0;

            if (source_data.source == nullptr ||
                source_data.row_count == 0 ||
                footprint.Footprint.Depth == 0 ||
                !upload_slice_pitch_valid ||
                !upload_size_valid ||
                source_data.row_size > source_data.source_row_pitch ||
                source_data.row_size > footprint.Footprint.RowPitch ||
                !source_slice_pitch_valid ||
                source_data.source_slice_pitch < minimum_source_slice_pitch ||
                subresource_upload_size > page_size_) {
                log::Logger::g_logger << log::abrt(
                    "Texture upload subresource is invalid.");
            }

            const UINT64 upload_offset =
                reserve_upload_space(
                    subresource_upload_size,
                    D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

            if (!transitioned_to_copy_dest) {
                destination.transition(
                    contexts_[current_page_].get(),
                    D3D12_RESOURCE_STATE_COPY_DEST);

                transitioned_to_copy_dest = true;
            }

            footprint.Offset = upload_offset;

            auto* destination_data =
                mapped_data_[current_page_] + upload_offset;

            for (UINT depth = 0;
                depth < footprint.Footprint.Depth;
                ++depth) {

                for (UINT row = 0;
                    row < source_data.row_count;
                    ++row) {

                    std::memcpy(
                        destination_data +
                        static_cast<std::size_t>(depth) *
                        static_cast<std::size_t>(upload_slice_pitch) +
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

            source_location.pResource =
                upload_buffers_[current_page_].get();
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

            cursors_[current_page_] =
                upload_offset + subresource_upload_size;
        }

        destination.transition(contexts_[current_page_].get(), final_state);
    }

    void ResourceUploader::flush() {
        if (!recording_) return;

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

                const auto val = contexts_[index].get_fence_value();
                if (val != 0) command_queue_->wait(val);
                if (upload_buffers_[index]) {
                    upload_buffers_[index]->Unmap(0, nullptr);
                    upload_buffers_[index].reset();
                }
                mapped_data_[index] = nullptr;
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
