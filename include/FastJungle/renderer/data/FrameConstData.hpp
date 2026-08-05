#pragma once

#include <cstdint>
#include <DirectXMath.h>

#include "FastJungle/dx12/MappedCBuffer.hpp"
#include "FastJungle/renderer/data/FrameConstData.hpp"

namespace fjr::render::data {

    struct FrameConstData {

        struct alignas(Consts::CBUF_ALIGN) CameraConstants {

            DirectX::XMFLOAT4X4 view_projection = Consts::I_MAT;
            DirectX::XMFLOAT3 world_position{};
            float padding_0 = 0.0f;

            DirectX::XMFLOAT4X4 environment_world_transform = Consts::I_MAT;
            DirectX::XMFLOAT3 environment_color{};
            float environment_intensity = 0.0f;

            uint32_t environment_texture_id = Consts::IND_ERR;
            uint32_t padding_1[3]{};
        };

        static_assert(sizeof(CameraConstants) == Consts::CBUF_ALIGN);
        static_assert(std::is_trivially_copyable_v<CameraConstants>);

        dx::MappedCBuffer<CameraConstants> camera_constants;
    };

}  // namespace fjr::render::data