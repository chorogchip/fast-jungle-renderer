#pragma once

#include <DirectXMath.h>
#include <d3d12.h>

#include <cstdint>
#include <type_traits>

#include "FastJungle/dx12/MappedCBuffer.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class Camera;

    class FrameData {

    public:
        void init(ID3D12Device* device);

        D3D12_GPU_VIRTUAL_ADDRESS get_camera_buffer() const noexcept;
        void upload_camera_data(
            const Camera& camera,
            const scene::StaticScene::EnvironmentLight& environment);

    private:
        struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
            CameraConstants {

            DirectX::XMFLOAT4X4 view_projection =
                scene::StaticScene::IDENTITY_TRANSFORM;
            DirectX::XMFLOAT3 world_position{};
            float padding_0 = 0.0f;

            DirectX::XMFLOAT4X4 environment_world_transform =
                scene::StaticScene::IDENTITY_TRANSFORM;
            DirectX::XMFLOAT3 environment_color{};
            float environment_intensity = 0.0f;

            std::uint32_t environment_texture_id =
                scene::StaticScene::INVALID_INDEX;
            std::uint32_t padding_1[3]{};
        };

        static_assert(sizeof(CameraConstants) ==
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        static_assert(std::is_trivially_copyable_v<CameraConstants>);

        dx::MappedCBuffer<CameraConstants> camera_buffer_;
    };

} // namespace fjr::render
