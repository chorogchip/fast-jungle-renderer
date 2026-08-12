#pragma once

#include "FastJungle/scene/StaticScene.hpp"

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace fjr::cooker::internal {

    enum class JungleComponent : uint32_t {
        ROOT,
        CAMERA,
        PYRAMID,
        RIVER,
        CREEK,
        BANYAN,
        TERRAIN_EXTENDED,
        TERRAIN_CINEMATIC,
        ANTHURIUM,
        NETTLE,
        SHRUB_SORREL,
        SHRUB,
        GRASS_B,
        GRASS_A,
        PYRAMID_GRASS_B,
        PYRAMID_MOSS,
        QUEEN_FOREST,
        RIVER_FOREST,
        RIVER_SAPLING,
        RIVER_SEEDLING,
        COUNT,
        UNKNOWN = UINT32_MAX,
    };

    [[nodiscard]] constexpr std::size_t component_index(
        JungleComponent component) noexcept {

        return static_cast<std::size_t>(component);
    }

    class JungleSceneProfile final {
    public:
        explicit JungleSceneProfile(const pxr::UsdStageRefPtr& stage);

        [[nodiscard]] JungleComponent component_for_prim(
            const pxr::UsdPrim& prim) const;

        [[nodiscard]] static bool is_point_component(
            JungleComponent component) noexcept;

        [[nodiscard]] static bool is_static_component(
            JungleComponent component) noexcept;

        [[nodiscard]] static scene::StaticScene::EnumPointCategory
        point_category(JungleComponent component);

    private:
        std::unordered_map<std::string, JungleComponent>
            source_layer_components_;
    };

} // namespace fjr::cooker::internal
