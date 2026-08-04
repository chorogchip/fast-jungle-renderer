#include "FastJungle/dx12/CommandContext.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::dx {

    void CommandContext::init(
        ID3D12Device* device,
        D3D12_COMMAND_LIST_TYPE type,
        UINT32 frame_index) {

        allocator_.Reset();
        command_list_.Reset();

        type_ = type;
        fence_value_ = 0;
        frame_index_ = frame_index;

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




    void CommandContext::RSSetViewPortScissorRect(UINT width, UINT height) {

        const D3D12_VIEWPORT viewport{
            0.0f,
            0.0f,
            static_cast<float>(width),
            static_cast<float>(height),
            0.0f,
            1.0f
        };
        const D3D12_RECT scissor{
            0,
            0,
            static_cast<LONG>(width),
            static_cast<LONG>(height)
        };
        command_list_->RSSetViewports(1, &viewport);
        command_list_->RSSetScissorRects(1, &scissor);
    }

    void CommandContext::SetDescriptorHeaps(
        ID3D12DescriptorHeap* heap1) {


        ID3D12DescriptorHeap* descriptor_heaps[]{
            heap1 };
        command_list_->SetDescriptorHeaps(1, descriptor_heaps);
    }

    void CommandContext::SetDescriptorHeaps(
        ID3D12DescriptorHeap* heap1,
        ID3D12DescriptorHeap* heap2) {


        ID3D12DescriptorHeap* descriptor_heaps[]{
            heap1, heap2 };
        command_list_->SetDescriptorHeaps(2, descriptor_heaps);
    }

    void CommandContext::SetDescriptorHeaps(
        ID3D12DescriptorHeap* heap1,
        ID3D12DescriptorHeap* heap2,
        ID3D12DescriptorHeap* heap3) {

        ID3D12DescriptorHeap* descriptor_heaps[]{
            heap1, heap2, heap3 };
        command_list_->SetDescriptorHeaps(3, descriptor_heaps);
    }

} // namespace fjr::dx