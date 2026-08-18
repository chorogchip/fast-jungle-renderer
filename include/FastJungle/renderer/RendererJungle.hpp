#pragma once

#include <array>
#include <cstdint>

#include "FastJungle/renderer/RendererBase.hpp"
#include "FastJungle/renderer/pass/PassGPUCull.hpp"
#include "FastJungle/renderer/pass/PassVisibility.hpp"
#include "FastJungle/renderer/pass/PassResolve.hpp"
#include "FastJungle/renderer/pass/PassSWRaster.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"

namespace fjr::render {

    class RendererJungle : public RendererBase {

    public:
        RendererJungle() = default;
        ~RendererJungle() = default;

        RendererJungle(const RendererJungle&) = delete;
        RendererJungle(RendererJungle&&) = delete;
        RendererJungle& operator=(const RendererJungle&) = delete;
        RendererJungle& operator=(RendererJungle&&) = delete;

        void init(
            void* window,
            uint32_t width, uint32_t height,
            const scene::StaticScene& scene);

        void resize(uint32_t width, uint32_t height);

        void render();

    private:
        void create_pass_views();
        void create_pass_targets(uint32_t width, uint32_t height);
        void update_software_resolve_views();

        PassGPUCull gpu_culling_pass_;
        PassSWRaster sw_raster_pass_;
        PassVisibility visibility_pass_;
        PassResolve resolve_pass_;

        data::DataPersistent data_persistent_;
        std::array<data::DataPerFrame, FRAME_COUNT> data_per_frame_;

        dx::Texture visibility_buffer_;
        dx::Texture frame_buffer_;

        std::array<dx::DescAlloc, FRAME_COUNT> visibility_input_views_;
        std::array<dx::DescAlloc, FRAME_COUNT> software_resolve_views_;

        dx::DescAlloc resolve_views_;
        dx::DescAlloc visibility_uav_;
        dx::DescAlloc visibility_clear_uav_;
        dx::DescAlloc visibility_rtv_;
        dx::DescAlloc frame_buffer_uav_;
        dx::DescAlloc frame_buffer_clear_uav_;
        dx::DescAlloc pass_samplers_;

    };

} // namespace fjr::render
