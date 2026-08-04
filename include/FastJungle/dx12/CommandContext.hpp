#pragma once

#include <wrl.h>
#include <d3d12.h>

namespace fjr::dx {

    class CommandContext {
    public:
        CommandContext() = default;

        CommandContext(const CommandContext&) = delete;
        CommandContext& operator=(const CommandContext&) = delete;

        CommandContext(CommandContext&&) noexcept = default;
        CommandContext& operator=(CommandContext&&) noexcept = default;

        void init(
            ID3D12Device* device,
            D3D12_COMMAND_LIST_TYPE type,
            UINT32 frame_index);

        void reset(
            ID3D12PipelineState* initial_pipeline_state = nullptr);

        void close();

        [[nodiscard]] ID3D12GraphicsCommandList* get() const noexcept {
            return command_list_.Get();
        }

        [[nodiscard]] ID3D12GraphicsCommandList* get_command_list() const noexcept {
            return command_list_.Get();
        }

        [[nodiscard]] ID3D12CommandAllocator* get_allocator() const noexcept {
            return allocator_.Get();
        }

        [[nodiscard]] D3D12_COMMAND_LIST_TYPE get_type() const noexcept {
            return type_;
        }

        [[nodiscard]] UINT64 get_fence_value() const noexcept {
            return fence_value_;
        }

        [[nodiscard]] UINT32 get_frame_index() const noexcept {
            return frame_index_;
        }

        void set_fence_value(UINT64 fence_value) noexcept {
            fence_value_ = fence_value;
        }

        [[nodiscard]] ID3D12GraphicsCommandList* operator->() const noexcept {
            return command_list_.Get();
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return allocator_ != nullptr && command_list_ != nullptr;
        }

        void RSSetViewPortScissorRect(UINT width, UINT height);

        void SetDescriptorHeaps(
            ID3D12DescriptorHeap* heap1);

        void SetDescriptorHeaps(
            ID3D12DescriptorHeap* heap1,
            ID3D12DescriptorHeap* heap2);

        void SetDescriptorHeaps(
            ID3D12DescriptorHeap* heap1,
            ID3D12DescriptorHeap* heap2,
            ID3D12DescriptorHeap* heap3);

    private:
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_;

        D3D12_COMMAND_LIST_TYPE type_ = D3D12_COMMAND_LIST_TYPE_DIRECT;
        UINT64 fence_value_ = 0;
        UINT32 frame_index_ = 0;
    };

} // namespace fjr::dx