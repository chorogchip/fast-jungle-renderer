#pragma once

#include <d3d12.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/Texture.hpp"

namespace fjr::dx {

    struct TextureSubresourceData {
        const std::byte* data = nullptr;
        UINT64 row_pitch = 0;
        UINT64 slice_pitch = 0;
    };

    // Alternates two caller-owned command lists so only two upload heaps can
    // be in flight. finish() waits for both and releases the heaps.
    class ResourceUploader final {
    public:
        static constexpr std::size_t COMMAND_LIST_COUNT = 2;

        ResourceUploader(
            ID3D12Device* device,
            CommandQueue& command_queue,
            std::array<
                CommandContext*,
                COMMAND_LIST_COUNT> command_lists);

        ResourceUploader(const ResourceUploader&) = delete;
        ResourceUploader& operator=(const ResourceUploader&) = delete;

        template<typename T>
        void upload_buffer(
            Buffer& destination,
            std::span<const T> source,
            D3D12_RESOURCE_STATES final_state) {

            static_assert(std::is_trivially_copyable_v<T>);
            upload_buffer_bytes(
                destination,
                std::as_bytes(source),
                final_state);
        }

        template<typename T>
        void upload_buffer_gathered(
            Buffer& destination,
            std::span<const T> source,
            std::span<const std::uint32_t> source_order,
            D3D12_RESOURCE_STATES final_state) {

            static_assert(std::is_trivially_copyable_v<T>);
            upload_buffer_gathered_bytes(
                destination,
                std::as_bytes(source),
                sizeof(T),
                source_order,
                final_state);
        }

        void upload_texture(
            Texture& destination,
            std::span<const TextureSubresourceData> source,
            D3D12_RESOURCE_STATES final_state);

        void finish();

    private:
        void upload_buffer_bytes(
            Buffer& destination,
            std::span<const std::byte> source,
            D3D12_RESOURCE_STATES final_state);

        void upload_buffer_gathered_bytes(
            Buffer& destination,
            std::span<const std::byte> source,
            std::size_t element_size,
            std::span<const std::uint32_t> source_order,
            D3D12_RESOURCE_STATES final_state);

        Buffer& acquire(UINT64 byte_size);
        void submit_current();

        ID3D12Device* device_ = nullptr;
        CommandQueue& command_queue_;
        std::array<
            CommandContext*,
            COMMAND_LIST_COUNT> command_lists_{};
        std::array<Buffer, COMMAND_LIST_COUNT> staging_;
        std::size_t current_list_ = 0;
        bool recording_ = false;
    };

} // namespace fjr::dx
