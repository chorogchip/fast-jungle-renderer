#include "FastJungle/dx12/Resource.hpp"

namespace fjr::dx {

    void Resource::transition(
        ID3D12GraphicsCommandList* command_list,
        D3D12_RESOURCE_STATES state) {

        if (state_ == state) {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

        barrier.Transition.pResource = resource_.Get();
        barrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = state_;
        barrier.Transition.StateAfter = state;

        command_list->ResourceBarrier(
            1,
            &barrier);

        state_ = state;
    }

} // namespace fjr::dx