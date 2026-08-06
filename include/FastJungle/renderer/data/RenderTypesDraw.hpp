#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    struct StbufDrawMetadata {
        uint32_t material_id = Consts::IND_ERR;
        uint32_t transform_index = Consts::IND_ERR;
        uint32_t index_count = 0;
        uint32_t first_index = 0;
        int32_t base_vertex = 0;
        EnumInstanceKind instance_kind = EnumInstanceKind::POINT;
        EnumRasterClass raster_class =
            EnumRasterClass::OPAQUE_SINGLE_SIDED;
        uint32_t padding = 0;
    };

    struct DrawFinalCPU {
        uint32_t draw_id = Consts::IND_ERR;
        uint32_t instance_offset = Consts::IND_ERR;
        EnumInstanceKind instance_kind = EnumInstanceKind::POINT;
        EnumRasterClass raster_class =
            EnumRasterClass::OPAQUE_SINGLE_SIDED;

        uint32_t offset_index = Consts::IND_ERR;
        uint32_t offset_vertex = Consts::IND_ERR;
        uint32_t count_index = 0;
        uint32_t count_instance = 0;
    };

    struct DrawFinalGPUIndirect {
        uint32_t draw_id = Consts::IND_ERR;
        uint32_t instance_offset = Consts::IND_ERR;
        EnumInstanceKind instance_kind = EnumInstanceKind::POINT;
        EnumRasterClass raster_class =
            EnumRasterClass::OPAQUE_SINGLE_SIDED;
        scene::StaticScene::EnumPointCategory point_category =
            scene::StaticScene::EnumPointCategory::COUNT;

        uint32_t offset_index = Consts::IND_ERR;
        uint32_t offset_vertex = Consts::IND_ERR;
        uint32_t count_index = 0;
        uint32_t count_instance = 0;

        uint32_t lod_index = 0;

        math::AABB world_bounds{};
        float world_scale = 1.0f;
        float lod_error = 0.0f;
        float next_lod_error =
            std::numeric_limits<float>::infinity();
    };

    static_assert(sizeof(StbufDrawMetadata) == 32);
    static_assert(std::is_trivially_copyable_v<StbufDrawMetadata>);
    static_assert(std::is_trivially_copyable_v<DrawFinalCPU>);
    static_assert(std::is_trivially_copyable_v<DrawFinalGPUIndirect>);
}
