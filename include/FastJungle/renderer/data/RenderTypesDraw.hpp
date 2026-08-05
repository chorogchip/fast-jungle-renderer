#pragma once

#include <cstdint>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data {

    enum class EnumDrawCpuFlag : uint32_t {
        DEFAULT = 0,
        DOUBLE_SIDED = 1u << 0,
        ALPHA_BLENDED = 1u << 1,
    };

    struct DrawRootConstants {
        uint32_t offset_instance = INVALID_INDEX;
        uint32_t offset_material = INVALID_INDEX;

        static constexpr inline uint32_t COUNT = 2;
    };

    struct DrawFinalCPU {
        DrawRootConstants constants{};
        EnumPointOrMatrix instnace_class = EnumPointOrMatrix::POINT;
        EnumPSOClass pso_class = EnumPSOClass::SIGLE_SIDED;
        uint32_t offset_cbuf_transform = Consts::IND_ERR;
        uint32_t offset_index = Consts::IND_ERR;
        uint32_t offset_vertex = Consts::IND_ERR;
        uint32_t count_index = 0;
        uint32_t count_instance = 0;
    };

    struct DrawFinalGPUIndirect {
        DrawRootConstants constants{};
        EnumPointOrMatrix instnace_class = EnumPointOrMatrix::POINT;
        EnumPSOClass pso_class = EnumPSOClass::SIGLE_SIDED;
        uint32_t offset_cbuf_transform = Consts::IND_ERR;
        uint32_t offset_index = Consts::IND_ERR;
        uint32_t offset_vertex = Consts::IND_ERR;
        uint32_t count_index = 0;
        uint32_t count_instance = 0;
        math::AABB world_bounds{};
        float lod_error = 0.0f;
        float next_lod_error = 0.0f;
    };

    static_assert(sizeof(DrawFinalGPUIndirect) == 64);
    static_assert(std::is_trivially_copyable_v<DrawFinalGPUIndirect>);
}