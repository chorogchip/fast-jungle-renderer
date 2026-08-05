#include "FastJungle/dx12/ResourceUploader.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace fjr::dx {

    ResourceUploader::ResourceUploader(
        ID3D12Device* device,
        CommandQueue& command_queue)
        : device_{device}, command_queue_{command_queue} {

        if (device_ == nullptr || !command_queue_) {
            throw std::invalid_argument(
                "ResourceUploader requires a device and command queue.");
        }

        context_.init(device_, command_queue_.get_type(), 0);
        context_.reset();
        staging_.reserve(STAGING_SLOT_COUNT);
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

        context_->CopyBufferRegion(
            destination.get(),
            0,
            upload.get(),
            0,
            byte_size);
        destination.transition(context_.get(), final_state);
    }

    void ResourceUploader::upload_texture(
        Texture& destination,
        std::span<const TextureSubresourceData> source,
        D3D12_RESOURCE_STATES final_state) {

        if (source.empty()) {
            return;
        }
        if (!destination) {
            throw std::invalid_argument(
                "Texture upload requires a destination resource.");
        }
        if (source.size() > std::numeric_limits<UINT>::max()) {
            throw std::overflow_error(
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
                throw std::invalid_argument(
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

            context_->CopyTextureRegion(
                &destination_location,
                0,
                0,
                0,
                &source_location,
                nullptr);
        }

        destination.transition(context_.get(), final_state);
    }

    void ResourceUploader::finish() {
        submit_and_reset();
    }

    Buffer& ResourceUploader::acquire(UINT64 byte_size) {
        if (byte_size == 0) {
            throw std::invalid_argument(
                "Upload buffer cannot be empty.");
        }
        if (staging_.size() == STAGING_SLOT_COUNT) {
            submit_and_reset();
        }

        staging_.emplace_back();
        auto& upload = staging_.back();
        upload.init(
            device_,
            byte_size,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        return upload;
    }

    void ResourceUploader::submit_and_reset() {
        if (staging_.empty()) {
            return;
        }

        context_.close();
        command_queue_.execute(context_.get());
        command_queue_.flush();
        staging_.clear();
        context_.reset();
    }

} // namespace fjr::dx
