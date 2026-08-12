#pragma once

#include <d3d12.h>

#include <cstddef>
#include <source_location>
#include <span>
#include <type_traits>
#include <vector>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/Texture.hpp"

namespace fjr::dx {

    struct TextureSubresourceData {
        std::span<const std::byte> data;
        UINT64 row_pitch = 0;
        UINT64 slice_pitch = 0;
    };

    class ResourceUploader final {
    public:
        ResourceUploader() = default;
        ~ResourceUploader();

        ResourceUploader(const ResourceUploader&) = delete;
        ResourceUploader& operator=(const ResourceUploader&) = delete;

        void init(
            ID3D12Device* device,
            CommandQueue& command_queue,
            std::size_t page_size,
            std::size_t page_count,
            std::source_location loc =
                std::source_location::current());

        void upload_buffer(
            Buffer& destination,
            std::span<const std::byte> source,
            D3D12_RESOURCE_STATES final_state,
            std::source_location loc =
                std::source_location::current());

        template<typename T, typename Allocator>
            requires (
                std::is_trivially_copyable_v<T> &&
                !std::is_same_v<std::remove_cv_t<T>, bool>)
        void upload_buffer(
            Buffer& destination,
            const std::vector<T, Allocator>& source,
            D3D12_RESOURCE_STATES final_state,
            std::source_location loc =
                std::source_location::current()) {

            upload_buffer(
                destination,
                std::as_bytes(std::span{ source }),
                final_state,
                loc);
        }

        void upload_texture(
            Texture& destination,
            std::span<const TextureSubresourceData> source,
            D3D12_RESOURCE_STATES final_state,
            std::source_location loc =
                std::source_location::current());

        void submit(
            std::source_location loc =
                std::source_location::current());

        void wait(
            std::source_location loc =
                std::source_location::current());

        void reset(
            std::source_location loc =
                std::source_location::current());

    private:
        void begin_recording(std::source_location loc);

        UINT64 reserve_upload_space(
            UINT64 size,
            UINT64 alignment,
            std::source_location loc);

        ID3D12Device* device_ = nullptr;
        CommandQueue* command_queue_ = nullptr;
        std::vector<CommandContext> contexts_;
        std::vector<Buffer> upload_buffers_;
        std::vector<std::byte*> mapped_data_;
        std::vector<UINT64> cursors_;
        UINT64 page_size_ = 0;
        std::size_t current_page_ = 0;
        bool recording_ = false;
        std::source_location source_location_{};
    };

} // namespace fjr::dx
