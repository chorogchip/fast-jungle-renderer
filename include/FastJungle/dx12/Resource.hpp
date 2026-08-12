#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <utility>

namespace fjr::dx {

    class CommandContext;

    class Resource {
    public:
        Resource() = default;

        Resource(const Resource&) = default;
        Resource& operator=(const Resource&) = default;

        Resource(Resource&&) noexcept = default;
        Resource& operator=(Resource&&) noexcept = default;

        [[nodiscard]]
        ID3D12Resource* get() const noexcept {
            return resource_.Get();
        }

        void reset() {
            resource_.Reset();
        }

        [[nodiscard]]
        ID3D12Resource* operator->() const noexcept {
            return resource_.Get();
        }

        [[nodiscard]]
        explicit operator bool() const noexcept {
            return resource_ != nullptr;
        }

    protected:
        void set_resource(
            Microsoft::WRL::ComPtr<ID3D12Resource> resource,
            D3D12_RESOURCE_STATES state) noexcept {

            resource_ = std::move(resource);
            state_ = state;
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> resource_;

    private:
        friend class CommandContext;

        D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;
    };

} // namespace fjr::dx
