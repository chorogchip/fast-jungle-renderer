#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <source_location>

namespace fjr::dx {

    class Resource;

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
            UINT32 frame_index,
            std::source_location loc =
                std::source_location::current());

        void reset(
            ID3D12PipelineState* initial_pipeline_state = nullptr,
            std::source_location loc =
                std::source_location::current());

        void close(
            std::source_location loc =
                std::source_location::current());

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

        void transition(
            Resource& resource,
            D3D12_RESOURCE_STATES state);

        void uav_barrier(const Resource& resource);

        void SetDescriptorHeaps(
            ID3D12DescriptorHeap* heap1);

        void SetDescriptorHeaps(
            ID3D12DescriptorHeap* heap1,
            ID3D12DescriptorHeap* heap2);

        void SetDescriptorHeaps(
            ID3D12DescriptorHeap* heap1,
            ID3D12DescriptorHeap* heap2,
            ID3D12DescriptorHeap* heap3);

        void Dispatch(
            UINT group_count_x,
            UINT group_count_y = 1,
            UINT group_count_z = 1) {

            command_list_->Dispatch(
                group_count_x,
                group_count_y,
                group_count_z);
        }

        template<UINT THREAD_COUNT_X>
        void Dispatch(UINT item_count) {
            static_assert(THREAD_COUNT_X > 0);

            Dispatch(
                div_round_up<THREAD_COUNT_X>(item_count),
                1,
                1);
        }

        template<UINT THREAD_COUNT_X, UINT THREAD_COUNT_Y>
        void Dispatch(UINT width, UINT height) {
            static_assert(THREAD_COUNT_X > 0);
            static_assert(THREAD_COUNT_Y > 0);

            Dispatch(
                div_round_up<THREAD_COUNT_X>(width),
                div_round_up<THREAD_COUNT_Y>(height),
                1);
        }

    private:
        template<UINT DIVISOR>
        [[nodiscard]]
        static constexpr UINT div_round_up(UINT value) noexcept {
            static_assert(DIVISOR > 0);
            return value / DIVISOR + (value % DIVISOR != 0);
        }

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_;

        D3D12_COMMAND_LIST_TYPE type_ = D3D12_COMMAND_LIST_TYPE_DIRECT;
        UINT64 fence_value_ = 0;
        UINT32 frame_index_ = 0;
    };

} // namespace fjr::dx
