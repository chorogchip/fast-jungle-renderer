#include "FastJungle/dx12/CommandContext.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::dx {

    void CommandContext::init(
        ID3D12Device* device,
        D3D12_COMMAND_LIST_TYPE type) {

        allocator_.Reset();
        command_list_.Reset();

        type_ = type;
        fence_value_ = 0;

        abort_failed(device->CreateCommandAllocator(
            type_,
            IID_PPV_ARGS(allocator_.ReleaseAndGetAddressOf())));

        abort_failed(device->CreateCommandList(
            0,
            type_,
            allocator_.Get(),
            nullptr,
            IID_PPV_ARGS(command_list_.ReleaseAndGetAddressOf())));

        abort_failed(command_list_->Close());
    }

    void CommandContext::reset(
        ID3D12PipelineState* initial_pipeline_state) {

        abort_failed(allocator_->Reset());

        abort_failed(command_list_->Reset(
            allocator_.Get(),
            initial_pipeline_state));
    }

    void CommandContext::close() {
        abort_failed(command_list_->Close());
    }

} // namespace fjr::dx