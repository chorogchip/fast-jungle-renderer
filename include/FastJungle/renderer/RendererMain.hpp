#pragma once

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/DeviceUtils.hpp"
#include "FastJungle/dx12/SwapChain.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/renderer/SceneResources.hpp"
#include "FastJungle/renderer/pass/ForwardPass.hpp"
#include "FastJungle/renderer/pass/VisibilityPass.hpp"
#include "FastJungle/renderer/pass/VisibilityResolvePass.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace fjr {

    class RendererMain {
    public:
        enum class RenderPath : std::uint32_t {
            FORWARD,
            VISIBILITY_BUFFER,
        };

        RendererMain() = default;
        ~RendererMain() = default;

        RendererMain(const RendererMain&) = delete;
        RendererMain(RendererMain&&) = delete;
        RendererMain& operator=(const RendererMain&) = delete;
        RendererMain& operator=(RendererMain&&) = delete;

        void init(
            void* window,
            std::uint32_t width,
            std::uint32_t height,
            const scene::StaticScene& scene);

        void resize(
            std::uint32_t width,
            std::uint32_t height);

        void render();
        void close();

        void set_render_path(RenderPath path) noexcept {
            render_path_ = path;
        }

        [[nodiscard]]
        RenderPath get_render_path() const noexcept {
            return render_path_;
        }

    private:
        static constexpr std::uint32_t FRAME_COUNT = 2;
        static constexpr std::uint32_t VISIBILITY_RTV_INDEX =
            FRAME_COUNT;

        void create_render_target_views();
        void create_depth_buffer(
            std::uint32_t width,
            std::uint32_t height);
        void create_visibility_buffer(
            std::uint32_t width,
            std::uint32_t height);
        void create_passes();
        void update_camera_constants(std::uint32_t frame_index);

        [[nodiscard]]
        render::ForwardPassView make_forward_view(
            std::uint32_t frame_index) const noexcept;

        [[nodiscard]]
        render::VisibilityPassView make_visibility_view(
            std::uint32_t frame_index) const noexcept;

        [[nodiscard]]
        render::VisibilityResolvePassView make_visibility_resolve_view(
            std::uint32_t frame_index) const noexcept;

        void record_forward(
            dx::CommandContext& context,
            std::uint32_t frame_index);
        void record_visibility_buffer(
            dx::CommandContext& context,
            std::uint32_t frame_index);

        Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
        Microsoft::WRL::ComPtr<ID3D12Device> device_;

        dx::SwapChain swap_chain_;
        dx::CommandQueue command_queue_;
        std::array<dx::CommandContext, FRAME_COUNT> command_contexts_;
        std::array<UINT64, FRAME_COUNT> frame_fence_values_{};

        dx::DescriptorHeap heap_rtv_;
        dx::DescriptorHeap heap_dsv_;
        dx::DescriptorHeap heap_visibility_srv_;
        dx::Texture depth_buffer_;
        dx::Texture visibility_buffer_;

        render::ForwardPass forward_pass_;
        render::VisibilityPass visibility_pass_;
        render::VisibilityResolvePass visibility_resolve_pass_;

        render::Camera camera_;
        std::unique_ptr<render::SceneResources> scene_resources_;
        RenderPath render_path_ = RenderPath::FORWARD;
    };

} // namespace fjr
