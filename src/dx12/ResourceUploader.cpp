#include "FastJungle/dx12/ResourceUploader.hpp"

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace fjr::dx {

    ResourceUploader::ResourceUploader(
        ID3D12Device* device,
        CommandQueue& command_queue,
        std::array<
            CommandContext*,
            COMMAND_LIST_COUNT> command_lists)
        : device_{device},
        command_queue_{command_queue},
        command_lists_{command_lists} {

        if (device_ == nullptr || !command_queue_) {
            log::Logger::g_logger << log::abrt(
                "ResourceUploader requires a device and command queue.");
        }

        for (const auto* command_list : command_lists_) {
            if (command_list == nullptr || !*command_list ||
                command_list->get_type() !=
                    command_queue_.get_type()) {

                log::Logger::g_logger << log::abrt(
                    "ResourceUploader requires two compatible command lists.");
            }
        }
    }

    void ResourceUploader::upload_buffer_bytes(
        Buffer& destination,
        std::span<const std::byte> source,
        D3D12_RESOURCE_STATES final_state) {

        if (source.empty()) {
            return;
        }

        const UINT64 byte_size = static_cast<UINT64>(source.size());
        destination.init(
            device_,
            byte_size,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST);

        auto& upload = acquire(byte_size);
        void* mapped = nullptr;
        const D3D12_RANGE read_range{0, 0};
        abort_failed(upload->Map(0, &read_range, &mapped));
        std::memcpy(mapped, source.data(), source.size());
        const D3D12_RANGE written_range{
            0,
            static_cast<SIZE_T>(byte_size),
        };
        upload->Unmap(0, &written_range);

        auto& context =
            *command_lists_[current_list_];

        context->CopyBufferRegion(
            destination.get(),
            0,
            upload.get(),
            0,
            byte_size);
        destination.transition(context.get(), final_state);
        submit_current();
    }

    void ResourceUploader::upload_buffer_gathered_bytes(
        Buffer& destination,
        std::span<const std::byte> source,
        std::size_t element_size,
        std::span<const std::uint32_t> source_order,
        D3D12_RESOURCE_STATES final_state) {

        if (source_order.empty()) {
            return;
        }
        if (element_size == 0 ||
            source.size() % element_size != 0) {

            log::Logger::g_logger << log::abrt(
                "Gathered buffer upload has an invalid element size.");
        }
        if (source_order.size() >
            std::numeric_limits<UINT64>::max() / element_size) {

            log::Logger::g_logger << log::abrt(
                "Gathered buffer upload is too large.");
        }

        const auto source_count =
            source.size() / element_size;

        for (const auto source_index : source_order) {
            if (source_index >= source_count) {
                log::Logger::g_logger << log::abrt(
                    "Gathered buffer upload has an invalid source index.");
            }
        }

        const UINT64 byte_size =
            static_cast<UINT64>(source_order.size()) *
            element_size;

        destination.init(
            device_,
            byte_size,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COPY_DEST);

        auto& upload = acquire(byte_size);
        void* mapped = nullptr;
        const D3D12_RANGE read_range{0, 0};
        abort_failed(upload->Map(0, &read_range, &mapped));

        auto* destination_data =
            static_cast<std::byte*>(mapped);

        for (std::size_t destination_index = 0;
            destination_index < source_order.size();
            ++destination_index) {

            std::memcpy(
                destination_data +
                    destination_index * element_size,
                source.data() +
                    static_cast<std::size_t>(
                        source_order[destination_index]) *
                    element_size,
                element_size);
        }

        const D3D12_RANGE written_range{
            0,
            static_cast<SIZE_T>(byte_size),
        };
        upload->Unmap(0, &written_range);

        auto& context =
            *command_lists_[current_list_];

        context->CopyBufferRegion(
            destination.get(),
            0,
            upload.get(),
            0,
            byte_size);
        destination.transition(context.get(), final_state);
        submit_current();
    }

    void ResourceUploader::upload_texture(
        Texture& destination,
        std::span<const TextureSubresourceData> source,
        D3D12_RESOURCE_STATES final_state) {

        if (source.empty()) {
            return;
        }
        if (!destination) {
            log::Logger::g_logger << log::abrt(
                "Texture upload requires a destination resource.");
        }
        if (source.size() > std::numeric_limits<UINT>::max()) {
            log::Logger::g_logger << log::abrt(
                "Texture upload has too many subresources.");
        }

        const UINT subresource_count = static_cast<UINT>(source.size());
        const auto description = destination->GetDesc();
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(
            subresource_count);
        std::vector<UINT> row_counts(subresource_count);
        std::vector<UINT64> row_sizes(subresource_count);
        UINT64 upload_size = 0;
        device_->GetCopyableFootprints(
            &description,
            0,
            subresource_count,
            0,
            footprints.data(),
            row_counts.data(),
            row_sizes.data(),
            &upload_size);

        for (UINT index = 0; index < subresource_count; ++index) {
            const auto minimum_slice_pitch =
                source[index].row_pitch * row_counts[index];
            if (source[index].data == nullptr ||
                source[index].row_pitch < row_sizes[index] ||
                source[index].slice_pitch < minimum_slice_pitch) {
                log::Logger::g_logger << log::abrt(
                    "Texture subresource layout is incompatible with D3D12.");
            }
        }

        auto& upload = acquire(upload_size);
        void* mapped = nullptr;
        const D3D12_RANGE read_range{0, 0};
        abort_failed(upload->Map(0, &read_range, &mapped));

        for (UINT index = 0; index < subresource_count; ++index) {
            const auto& source_data = source[index];
            const auto& footprint = footprints[index];
            auto* destination_data = static_cast<std::byte*>(mapped) +
                footprint.Offset;
            const UINT64 destination_slice_pitch =
                static_cast<UINT64>(footprint.Footprint.RowPitch) *
                row_counts[index];

            for (UINT depth = 0;
                depth < footprint.Footprint.Depth;
                ++depth) {
                for (UINT row = 0; row < row_counts[index]; ++row) {
                    std::memcpy(
                        destination_data +
                            static_cast<std::size_t>(depth) *
                                destination_slice_pitch +
                            static_cast<std::size_t>(row) *
                                footprint.Footprint.RowPitch,
                        source_data.data +
                            static_cast<std::size_t>(depth) *
                                source_data.slice_pitch +
                            static_cast<std::size_t>(row) *
                                source_data.row_pitch,
                        static_cast<std::size_t>(row_sizes[index]));
                }
            }
        }

        const D3D12_RANGE written_range{
            0,
            static_cast<SIZE_T>(upload_size),
        };
        upload->Unmap(0, &written_range);

        for (UINT index = 0; index < subresource_count; ++index) {
            D3D12_TEXTURE_COPY_LOCATION destination_location{};
            destination_location.pResource = destination.get();
            destination_location.Type =
                D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination_location.SubresourceIndex = index;

            D3D12_TEXTURE_COPY_LOCATION source_location{};
            source_location.pResource = upload.get();
            source_location.Type =
                D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source_location.PlacedFootprint = footprints[index];

            command_lists_[current_list_]->get()
                ->CopyTextureRegion(
                &destination_location,
                0,
                0,
                0,
                &source_location,
                nullptr);
        }

        destination.transition(
            command_lists_[current_list_]->get(),
            final_state);
        submit_current();
    }

    void ResourceUploader::finish() {
        submit_current();

        for (std::size_t index = 0;
            index < COMMAND_LIST_COUNT;
            ++index) {

            if (!staging_[index]) {
                continue;
            }

            command_queue_.wait(
                command_lists_[index]
                    ->get_fence_value());
            staging_[index].reset();
        }
    }

    Buffer& ResourceUploader::acquire(UINT64 byte_size) {
        if (byte_size == 0) {
            log::Logger::g_logger << log::abrt(
                "Upload buffer cannot be empty.");
        }
        if (recording_) {
            log::Logger::g_logger << log::abrt(
                "ResourceUploader already records an upload.");
        }

        auto& context =
            *command_lists_[current_list_];

        command_queue_.wait(
            context.get_fence_value());

        auto& upload = staging_[current_list_];
        upload.reset();
        context.reset();
        recording_ = true;

        upload.init(
            device_,
            byte_size,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        return upload;
    }

    void ResourceUploader::submit_current() {
        if (!recording_) {
            return;
        }

        auto& context =
            *command_lists_[current_list_];

        context.close();
        command_queue_.execute(context.get());
        context.set_fence_value(
            command_queue_.signal());

        recording_ = false;
        current_list_ =
            (current_list_ + 1) % COMMAND_LIST_COUNT;
    }

} // namespace fjr::dx
