#pragma once

#include <d3d12.h>

#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

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

    // Records uploads in small batches so temporary upload heaps never
    // accumulate for the lifetime of all destination resources.
    class ResourceUploader final {
    public:
        static constexpr std::size_t STAGING_SLOT_COUNT = 2;

        ResourceUploader(
            ID3D12Device* device,
            CommandQueue& command_queue);

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

        Buffer& acquire(UINT64 byte_size);
        void submit_and_reset();

        ID3D12Device* device_ = nullptr;
        CommandQueue& command_queue_;
        CommandContext context_;
        std::vector<Buffer> staging_;
    };

} // namespace fjr::dx
