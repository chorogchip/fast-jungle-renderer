#pragma once

/*
Renderer가 StaticScene을 가공 시 CPU/GPU 양측에서 처리하고 버퍼를 만듬
여기에는 CPU/GPU 양측에서 알아야 할 공용 타입 정의
중간에 만들고 없앨 타입은 가능하면 다른 곳에서 씀
*/

#include <cstdint>
#include <DirectXMath.h>

#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data {

    struct alignas(Consts::CBUF_ALIGN) CbufPointDraw {
        DirectX::XMFLOAT4X4 part_local_transform = Consts::I_MAT;
        DirectX::XMFLOAT4X4 batch_local_to_world = Consts::I_MAT;
    };

    /*
        StaticScene의 행렬들 하나로 합침.
        normal 변환까지 이 행렬로 처리함. (uniform scale 가정)
    */
    struct alignas(Consts::CBUF_ALIGN) CbufMatrixDraw {
        DirectX::XMFLOAT4X4 part_local_transform =
            scene::StaticScene::IDENTITY_TRANSFORM;
    };

    /*
        StaticScene에서 matrix만 분리해서 씀.
    */
    struct StbufMatrixInstance {
        DirectX::XMFLOAT4X4 transform = Consts::I_MAT;
    };

    struct StbufMaterial {
        DirectX::XMFLOAT4 base_color{ 0.18f, 0.18f, 0.18f, 1.0f };
        DirectX::XMFLOAT4 emissive_roughness{ 0.0f, 0.0f, 0.0f, 0.5f };
        DirectX::XMFLOAT4 surface{ 0.0f, 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 optical{ 0.5f, 0.0f, 0.01f, 0.0f };

        uint32_t texture_binding_basecolor = Consts::IND_ERR;
        uint32_t texture_binding_normal = Consts::IND_ERR;
        uint32_t texture_binding_roughness = Consts::IND_ERR;
        uint32_t texture_binding_opacity = Consts::IND_ERR;
        uint32_t texture_binding_emissive = Consts::IND_ERR;
        uint32_t texture_binding_metallic = Consts::IND_ERR;
        uint32_t texture_binding_reserved0 = Consts::IND_ERR;
        uint32_t texture_binding_reserved1 = Consts::IND_ERR;
    };

    struct StbufTextureBinding {
        uint32_t texture_id = scene::StaticScene::INVALID_INDEX;
        uint32_t sampler_id = scene::StaticScene::INVALID_INDEX;
        uint32_t channel = 0;
        uint32_t flags = 0;  // not used
    };


    static_assert(sizeof(CbufPointDraw) == Consts::CBUF_ALIGN);
    static_assert(sizeof(CbufMatrixDraw) == Consts::CBUF_ALIGN);
    static_assert(sizeof(StbufMaterial) == 96);
    static_assert(sizeof(StbufTextureBinding) == 16);

    static_assert(std::is_trivially_copyable_v<CbufPointDraw>);
    static_assert(std::is_trivially_copyable_v<CbufMatrixDraw>);
    static_assert(std::is_trivially_copyable_v<StbufMatrixInstance>);
    static_assert(std::is_trivially_copyable_v<StbufMaterial>);
    static_assert(std::is_trivially_copyable_v<StbufTextureBinding>);
}
