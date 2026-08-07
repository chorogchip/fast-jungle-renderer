#pragma once

#include <d3d12.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/Texture.hpp"

namespace fjr::dx {

    struct TextureSubresourceData {
        const std::byte* source = nullptr;
        UINT64 source_row_pitch = 0;
        UINT64 source_slice_pitch = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT base_footprint{};
        UINT row_count = 0;
        UINT64 row_size = 0;
    };

    struct TextureUploadDesc {
        std::span<const TextureSubresourceData> subresources;
        UINT64 required_upload_size = 0;
    };

    class ResourceUploader final {
    public:
        ResourceUploader() = default;

        ResourceUploader(const ResourceUploader&) = delete;
        ResourceUploader& operator=(const ResourceUploader&) = delete;

        void init(
            ID3D12Device* device,
            CommandQueue& command_queue,
            std::size_t page_size,
            std::size_t page_count);

        void upload_buffer(
            Buffer& destination,
            std::span<const std::byte> source,
            D3D12_RESOURCE_STATES final_state);

        void upload_buffer_gathered(
            Buffer& destination,
            std::span<const std::byte> source,
            std::size_t element_size,
            std::span<const std::uint32_t> source_order,
            D3D12_RESOURCE_STATES final_state);

        void upload_texture(
            Texture& destination,
            const TextureUploadDesc& source,
            D3D12_RESOURCE_STATES final_state);

        void flush();
        void reset();

    private:
        void begin_recording();

        UINT64 reserve_upload_space(
            UINT64 size,
            UINT64 alignment);

        CommandQueue* command_queue_ = nullptr;
        std::vector<CommandContext> contexts_;
        std::vector<Buffer> upload_buffers_;
        std::vector<std::byte*> mapped_data_;
        std::vector<UINT64> cursors_;
        UINT64 page_size_ = 0;
        std::size_t current_page_ = 0;
        bool recording_ = false;
    };

} // namespace fjr::dx
