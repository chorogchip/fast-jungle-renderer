#pragma once

struct ID3D12Device;

#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/renderer/data/FrameConstData.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {
    class SceneFrameConstDataBuilder final {
    public:
        SceneFrameConstDataBuilder() = delete;

        static void build(
            data::FrameConstData& output,
            ID3D12Device* device,
            const Camera& camera,
            const scene::StaticScene::EnvironmentLight& environment);
    };
} // namespace fjr::render
