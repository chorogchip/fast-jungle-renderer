#pragma once

#include <cstdint>
#include <DirectXMath.h>

#include "FastJungle/dx12/MappedCBuffer.hpp"
#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    struct DataPerFrame {

        struct alignas(Consts::CBUF_ALIGN) CameraConstants {
            DirectX::XMFLOAT4X4 view_projection = Consts::I_MAT;
            DirectX::XMFLOAT3 world_position{};
            float padding_0 = 0.0f;

            DirectX::XMFLOAT4X4 environment_world_transform =
                Consts::I_MAT;
            DirectX::XMFLOAT3 environment_color{};
            float environment_intensity = 0.0f;

            uint32_t environment_texture_id = Consts::IND_ERR;
        };

        dx::MappedCBuffer<CameraConstants> camera_constants;
        bool initialized = false;

        static DataPerFrame build(
            ID3D12Device* device,
            const Camera& camera,
            const scene::StaticScene::EnvironmentLight& environment);
    };

}  // namespace fjr::render::data