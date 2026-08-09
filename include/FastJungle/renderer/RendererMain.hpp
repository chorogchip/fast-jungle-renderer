#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "FastJungle/renderer/RendererBase.hpp"
#include "FastJungle/renderer/pass/GpuCullingPass.hpp"
#include "FastJungle/renderer/pass/ForwardPass.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"

namespace fjr::render {

    class RendererMain : public RendererBase {

    public:
        RendererMain() = default;
        ~RendererMain() = default;

        RendererMain(const RendererMain&) = delete;
        RendererMain(RendererMain&&) = delete;
        RendererMain& operator=(const RendererMain&) = delete;
        RendererMain& operator=(RendererMain&&) = delete;

        void init(
            void* window,
            uint32_t width, uint32_t height,
            const scene::StaticScene& scene);

        void resize(uint32_t width, uint32_t height);

        void render();

    private:
        GpuCullingPass gpu_culling_pass_;
        ForwardPass forward_pass_;

        data::DataPersistent data_persistant_;
        std::array<data::DataPerFrame, FRAME_COUNT> data_per_frame_;

        struct ImpostorProbeTarget final {
            std::string name;
            uint64_t total_instance_count = 0;
            std::vector<uint32_t> mesh_lod_ids;
            std::vector<uint32_t> triangles_per_lod;
        };

        std::vector<ImpostorProbeTarget> impostor_probe_targets_;
        std::array<bool, FRAME_COUNT> impostor_probe_readback_ready_{};
        uint32_t impostor_probe_readback_count_ = 0;

        void log_impostor_probe(
            const data::DataPerFrame& frame) const;
    };

} // namespace fjr
