#include "FastJungle/dx12/ResourceUploader.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::dx {

    namespace {

        constexpr UINT64 MIN_UPLOAD_PAGE_SIZE =
            D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION * 16ull;

        [[nodiscard]]
        UINT texture_row_height(
            DXGI_FORMAT format,
            UINT height,
            UINT row_count) noexcept {

            switch (format) {
            case DXGI_FORMAT_BC1_TYPELESS:
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
            case DXGI_FORMAT_BC2_TYPELESS:
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
            case DXGI_FORMAT_BC3_TYPELESS:
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
            case DXGI_FORMAT_BC4_TYPELESS:
            case DXGI_FORMAT_BC4_UNORM:
            case DXGI_FORMAT_BC4_SNORM:
            case DXGI_FORMAT_BC5_TYPELESS:
            case DXGI_FORMAT_BC5_UNORM:
            case DXGI_FORMAT_BC5_SNORM:
            case DXGI_FORMAT_BC6H_TYPELESS:
            case DXGI_FORMAT_BC6H_UF16:
            case DXGI_FORMAT_BC6H_SF16:
            case DXGI_FORMAT_BC7_TYPELESS:
            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                return 4;

            default:
                return (height + row_count - 1) / row_count;
            }
        }

    } // namespace

    ResourceUploader::~ResourceUploader() {
        reset(source_location_);
    }

    void ResourceUploader::init(
        ID3D12Device* device,
        CommandQueue& command_queue,
        std::size_t page_size,
        std::size_t page_count,
        std::source_location loc) {

        if (device == nullptr || page_size == 0 || page_count == 0) {
            log::Logger::g_logger << log::abrt(
                "ResourceUploader requires a device and non-zero pages.",
                loc);
        }

        this->reset(loc);

        device_ = device;
        command_queue_ = &command_queue;
        source_location_ = loc;
        page_size_ = (std::max)(
            static_cast<UINT64>(page_size),
            MIN_UPLOAD_PAGE_SIZE);
        current_page_ = 0;
        recording_ = false;

        contexts_.resize(page_count);
        upload_buffers_.resize(page_count);
        mapped_data_.resize(page_count, nullptr);
        cursors_.resize(page_count, 0);

        for (std::size_t index = 0; index < page_count; ++index) {
            auto& context = contexts_[index];
            auto& upload_buffer = upload_buffers_[index];

            context.init(
                device,
                command_queue.get_type(),
                static_cast<UINT32>(index),
                loc);

            upload_buffer.init(
                device,
                page_size_,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                loc);

            void* mapped = nullptr;
            const D3D12_RANGE read_range{ 0, 0 };

            abort_failed(upload_buffer->Map(0, &read_range, &mapped), loc);
            mapped_data_[index] = static_cast<std::byte*>(mapped);
        }
    }

    void ResourceUploader::begin_recording(std::source_location loc) {
        if (recording_) return;

        auto& context = contexts_[current_page_];
        const UINT64 fence_value = context.get_fence_value();

        if (fence_value != 0) command_queue_->wait(fence_value, loc);

        context.reset(nullptr, loc);
        cursors_[current_page_] = 0;
        recording_ = true;
    }

    UINT64 ResourceUploader::reserve_upload_space(
        UINT64 size,
        UINT64 alignment,
        std::source_location loc) {
        if (alignment == 0 || size > page_size_) {
            log::Logger::g_logger << log::abrt(
                "ResourceUploader reservation is invalid.",
                loc);
        }

        int i = 0;
        do {
            this->begin_recording(loc);

            const UINT64 remainder = cursors_[current_page_] % alignment;
            UINT64 offset = cursors_[current_page_];
            if (remainder != 0) offset += alignment - remainder;

            if (offset <= page_size_ && size <= page_size_ - offset)
                return offset;

            if (i++) break;

            this->submit(loc);
        } while (true);

        log::Logger::g_logger << log::abrt({}, loc);
        return -1;
    }

    void ResourceUploader::upload_buffer(
        Buffer& destination,
        std::span<const std::byte> source,
        D3D12_RESOURCE_STATES final_state,
        std::source_location loc) {

        this->begin_recording(loc);

        if (cursors_[current_page_] == page_size_) {
            this->submit(loc);
            this->begin_recording(loc);
        }

        contexts_[current_page_].transition(
            destination,
            D3D12_RESOURCE_STATE_COPY_DEST);

        UINT64 destination_offset = 0;
        const UINT64 byte_size = static_cast<UINT64>(source.size());

        while (destination_offset < byte_size) {
            this->begin_recording(loc);

            const UINT64 available = page_size_ - cursors_[current_page_];

            if (available == 0) {
                this->submit(loc);
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

        contexts_[current_page_].transition(destination, final_state);
    }

    void ResourceUploader::upload_texture(
        Texture& destination,
        std::span<const TextureSubresourceData> source,
        D3D12_RESOURCE_STATES final_state,
        std::source_location loc) {

        const UINT subresource_count = static_cast<UINT>(source.size());
        const D3D12_RESOURCE_DESC description = destination->GetDesc();

        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(
            subresource_count);
        std::vector<UINT> row_counts(subresource_count);
        std::vector<UINT64> row_sizes(subresource_count);

        device_->GetCopyableFootprints(
            &description,
            0,
            subresource_count,
            0,
            footprints.data(),
            row_counts.data(),
            row_sizes.data(),
            nullptr);

        bool transitioned_to_copy_dest = false;

        for (UINT index = 0;
            index < subresource_count;
            ++index) {

            const auto& source_data = source[index];
            const auto& footprint = footprints[index];
            const UINT row_count = row_counts[index];
            const UINT64 row_size = row_sizes[index];
            const UINT row_height = texture_row_height(
                footprint.Footprint.Format,
                footprint.Footprint.Height,
                row_count);
            const UINT rows_per_chunk = static_cast<UINT>((std::max)(
                UINT64{1},
                (std::min)(
                    static_cast<UINT64>(row_count),
                    page_size_ / footprint.Footprint.RowPitch)));

            for (UINT depth = 0;
                depth < footprint.Footprint.Depth;
                ++depth) {

                for (UINT first_row = 0;
                    first_row < row_count;
                    first_row += rows_per_chunk) {

                    const UINT chunk_row_count = (std::min)(
                        rows_per_chunk,
                        row_count - first_row);
                    const UINT64 upload_size =
                        static_cast<UINT64>(footprint.Footprint.RowPitch) *
                        chunk_row_count;
                    const UINT64 upload_offset = reserve_upload_space(
                        upload_size,
                        D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT,
                        loc);

                    if (!transitioned_to_copy_dest) {
                        contexts_[current_page_].transition(
                            destination,
                            D3D12_RESOURCE_STATE_COPY_DEST);
                        transitioned_to_copy_dest = true;
                    }

                    auto* destination_data =
                        mapped_data_[current_page_] + upload_offset;
                    const auto* source_data_begin =
                        source_data.data.data() +
                        static_cast<std::size_t>(depth) *
                        source_data.slice_pitch +
                        static_cast<std::size_t>(first_row) *
                        source_data.row_pitch;

                    for (UINT row = 0;
                        row < chunk_row_count;
                        ++row) {

                        std::memcpy(
                            destination_data +
                            static_cast<std::size_t>(row) *
                            footprint.Footprint.RowPitch,
                            source_data_begin +
                            static_cast<std::size_t>(row) *
                            source_data.row_pitch,
                            static_cast<std::size_t>(row_size));
                    }

                    const UINT destination_y = first_row * row_height;
                    auto chunk_footprint = footprint;
                    chunk_footprint.Offset = upload_offset;
                    chunk_footprint.Footprint.Height = (std::min)(
                        chunk_row_count * row_height,
                        footprint.Footprint.Height - destination_y);
                    chunk_footprint.Footprint.Depth = 1;

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
                    source_location.PlacedFootprint = chunk_footprint;

                    contexts_[current_page_]->CopyTextureRegion(
                        &destination_location,
                        0,
                        destination_y,
                        depth,
                        &source_location,
                        nullptr);

                    cursors_[current_page_] = upload_offset + upload_size;
                }
            }
        }

        contexts_[current_page_].transition(destination, final_state);
    }

    void ResourceUploader::submit(std::source_location loc) {
        if (!recording_) return;

        auto& context = contexts_[current_page_];

        context.close(loc);
        command_queue_->execute(context.get());
        context.set_fence_value(command_queue_->signal(loc));

        recording_ = false;
        current_page_ = (current_page_ + 1) % contexts_.size();
    }

    void ResourceUploader::wait(std::source_location loc) {
        submit(loc);

        for (auto& context : contexts_) {
            const UINT64 fence_value = context.get_fence_value();
            if (fence_value == 0) continue;

            command_queue_->wait(fence_value, loc);
            context.set_fence_value(0);
        }
    }

    void ResourceUploader::reset(std::source_location loc) {
        if (command_queue_ != nullptr) {
            wait(loc);

            for (std::size_t index = 0; index < contexts_.size(); ++index) {
                if (upload_buffers_[index]) {
                    upload_buffers_[index]->Unmap(0, nullptr);
                    upload_buffers_[index].reset();
                }
                mapped_data_[index] = nullptr;
                contexts_[index] = CommandContext{};
            }
        }
        device_ = nullptr;
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
