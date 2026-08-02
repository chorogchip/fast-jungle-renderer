#include "FastJungle/dx12/CommandQueue.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

#include <cstdlib>
#include <utility>

namespace fjr::dx {

    CommandQueue::~CommandQueue() {
        if (fence_event_) {
            CloseHandle(fence_event_);
        }
    }

    CommandQueue::CommandQueue(CommandQueue&& other) noexcept
        : command_queue_(std::move(other.command_queue_)),
        fence_(std::move(other.fence_)),
        fence_event_(std::exchange(other.fence_event_, nullptr)),
        fence_value_(std::exchange(other.fence_value_, 0)),
        type_(other.type_) {}

    CommandQueue& CommandQueue::operator=(CommandQueue&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (fence_event_) {
            CloseHandle(fence_event_);
        }

        command_queue_ = std::move(other.command_queue_);
        fence_ = std::move(other.fence_);

        fence_event_ = std::exchange(other.fence_event_, nullptr);
        fence_value_ = std::exchange(other.fence_value_, 0);
        type_ = other.type_;

        return *this;
    }

    void CommandQueue::init(
        ID3D12Device* device,
        D3D12_COMMAND_LIST_TYPE type) {

        if (fence_event_) {
            CloseHandle(fence_event_);
            fence_event_ = nullptr;
        }

        command_queue_.Reset();
        fence_.Reset();

        type_ = type;
        fence_value_ = 0;

        D3D12_COMMAND_QUEUE_DESC description{};
        description.Type = type_;
        description.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        description.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        description.NodeMask = 0;

        abort_failed(device->CreateCommandQueue(
            &description,
            IID_PPV_ARGS(command_queue_.ReleaseAndGetAddressOf())));

        abort_failed(device->CreateFence(
            fence_value_,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf())));

        fence_event_ = CreateEvent(
            nullptr,
            FALSE,
            FALSE,
            nullptr);

        if (!fence_event_) {
            abort_failed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }

    void CommandQueue::execute(
        std::span<ID3D12CommandList* const> command_lists) {

        if (command_lists.empty()) {
            return;
        }

        command_queue_->ExecuteCommandLists(
            static_cast<UINT>(command_lists.size()),
            command_lists.data());
    }

    UINT64 CommandQueue::signal() {
        const UINT64 value = ++fence_value_;

        abort_failed(command_queue_->Signal(
            fence_.Get(),
            value));

        return value;
    }

    void CommandQueue::wait(UINT64 fence_value) {
        if (fence_->GetCompletedValue() >= fence_value) {
            return;
        }

        abort_failed(fence_->SetEventOnCompletion(
            fence_value,
            fence_event_));

        WaitForSingleObject(
            fence_event_,
            INFINITE);
    }

    void CommandQueue::flush() {
        wait(signal());
    }

    void CommandQueue::wait(
        const CommandQueue& other,
        UINT64 fence_value) {

        abort_failed(command_queue_->Wait(
            other.fence_.Get(),
            fence_value));
    }

} // namespace fjr::dx
