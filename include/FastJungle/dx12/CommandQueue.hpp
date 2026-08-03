#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>

#include <span>

namespace fjr::dx {

    class CommandQueue {
    public:
        CommandQueue() = default;
        ~CommandQueue();

        CommandQueue(const CommandQueue&) = delete;
        CommandQueue& operator=(const CommandQueue&) = delete;

        CommandQueue(CommandQueue&& other) noexcept;
        CommandQueue& operator=(CommandQueue&& other) noexcept;

        void init(
            ID3D12Device* device,
            D3D12_COMMAND_LIST_TYPE type);

        void execute(ID3D12CommandList* command_list);
        void execute(ID3D12CommandList* const* command_lists, UINT count);

        [[nodiscard]] UINT64 signal();

        void wait(UINT64 fence_value);
        void flush();

        void wait(
            const CommandQueue& other,
            UINT64 fence_value);

        [[nodiscard]] ID3D12CommandQueue* get_command_queue() const noexcept {
            return command_queue_.Get();
        }

        [[nodiscard]] ID3D12Fence* get_fence() const noexcept {
            return fence_.Get();
        }

        [[nodiscard]] D3D12_COMMAND_LIST_TYPE get_type() const noexcept {
            return type_;
        }

        [[nodiscard]] UINT64 get_fence_value() const noexcept {
            return fence_value_;
        }

        [[nodiscard]] UINT64 get_completed_value() const noexcept {
            return fence_ ? fence_->GetCompletedValue() : 0;
        }

        [[nodiscard]] ID3D12CommandQueue* operator->() const noexcept {
            return command_queue_.Get();
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return command_queue_ != nullptr;
        }

    private:
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
        Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

        HANDLE fence_event_ = nullptr;
        UINT64 fence_value_ = 0;

        D3D12_COMMAND_LIST_TYPE type_ = D3D12_COMMAND_LIST_TYPE_DIRECT;
    };

} // namespace fjr::dx