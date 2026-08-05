#include "JungleSceneProfile.hpp"

#include "CookError.hpp"
#include "PathKey.hpp"

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string_view>

namespace fjr::cooker::internal {

    namespace {

        constexpr std::array POINT_COMPONENTS{
            JungleComponent::ANTHURIUM,
            JungleComponent::NETTLE,
            JungleComponent::SHRUB_SORREL,
            JungleComponent::SHRUB,
            JungleComponent::GRASS_B,
            JungleComponent::GRASS_A,
            JungleComponent::PYRAMID_GRASS_B,
            JungleComponent::PYRAMID_MOSS,
            JungleComponent::QUEEN_FOREST,
            JungleComponent::RIVER_FOREST,
            JungleComponent::RIVER_SAPLING,
            JungleComponent::RIVER_SEEDLING,
        };

        [[nodiscard]] std::optional<JungleComponent> component_for_sublayer(
            std::string_view path) {

            static const std::unordered_map<std::string, JungleComponent>
                values{
                    {"cameras/defaultCam.usda", JungleComponent::CAMERA},
                    {"elements/Anthurium/PI_Anthurium.usd", JungleComponent::ANTHURIUM},
                    {"elements/Nettle/PI_Nettle.usd", JungleComponent::NETTLE},
                    {"elements/ShrubSorrel/PI_S_ShrubSorrel.usd", JungleComponent::SHRUB_SORREL},
                    {"elements/Shrub/PI_Shrub.usd", JungleComponent::SHRUB},
                    {"elements/Grass_B/PI_Grass_B.usd", JungleComponent::GRASS_B},
                    {"elements/Grass_A/PI_Grass_A.usd", JungleComponent::GRASS_A},
                    {"elements/Pyramid_Grass_B/PI_Pyramid_GrassB.usd", JungleComponent::PYRAMID_GRASS_B},
                    {"elements/Pyramid_Moss/PI_S_Moss.usd", JungleComponent::PYRAMID_MOSS},
                    {"elements/QueenForest/PI_S_QueenForest.usd", JungleComponent::QUEEN_FOREST},
                    {"elements/RiverForest/PI_S_RiverForest.usd", JungleComponent::RIVER_FOREST},
                    {"elements/RiverSapling/PI_RiverSapling.usd", JungleComponent::RIVER_SAPLING},
                    {"elements/RiverSeedling/PI_S_RiverSeedling.usd", JungleComponent::RIVER_SEEDLING},
                    {"elements/Creek/Creek.usd", JungleComponent::CREEK},
                    {"elements/Banyan/Banyan.usd", JungleComponent::BANYAN},
                    {"elements/Pyramid/Pyramid.usd", JungleComponent::PYRAMID},
                    {"elements/River/River.usd", JungleComponent::RIVER},
                    {"elements/Terrain/Terrain_Extended.usd", JungleComponent::TERRAIN_EXTENDED},
                    {"elements/Terrain/Terrain_Cinematic.usd", JungleComponent::TERRAIN_CINEMATIC},
                };

            auto normalized = std::filesystem::path{path}.lexically_normal()
                .generic_string();
            if (normalized.starts_with("./")) {
                normalized.erase(0, 2);
            }
            const auto found = values.find(normalized);
            return found == values.end()
                ? std::nullopt
                : std::optional{found->second};
        }

    } // namespace

    JungleSceneProfile::JungleSceneProfile(
        const pxr::UsdStageRefPtr& stage) {

        const auto root = stage->GetRootLayer();
        if (!root) {
            fail("OpenUSD stage has no root layer.");
        }

        const std::filesystem::path root_path{root->GetRealPath()};
        std::array<bool, component_index(JungleComponent::COUNT)> seen{};
        source_layer_components_.emplace(
            normalized_path_key(root_path),
            JungleComponent::ROOT);
        seen[component_index(JungleComponent::ROOT)] = true;

        const auto root_directory = root_path.parent_path();
        for (const auto& authored : root->GetSubLayerPaths()) {
            const auto authored_string = static_cast<std::string>(authored);
            const auto component = component_for_sublayer(authored_string);
            if (!component) {
                fail(
                    "Unexpected direct Jungle root sublayer: ",
                    authored_string);
            }
            const auto index = component_index(*component);
            if (seen[index]) {
                fail(
                    "Duplicate Jungle root component sublayer: ",
                    authored_string);
            }
            seen[index] = true;

            const auto real_path =
                (root_directory / std::filesystem::path{authored_string})
                    .lexically_normal();
            source_layer_components_.emplace(
                normalized_path_key(real_path),
                *component);
        }

        if (std::ranges::find(seen, false) != seen.end()) {
            fail("Jungle root USDA is missing a required direct sublayer.");
        }
    }

    JungleComponent JungleSceneProfile::component_for_prim(
        const pxr::UsdPrim& prim) const {

        // A referenced asset may expose internal child layers. The nearest
        // ancestor authored by a direct root sublayer is the semantic owner.
        for (auto current = prim; current; current = current.GetParent()) {
            bool authored_in_root = false;
            for (const auto& spec : current.GetPrimStack()) {
                if (!spec || !spec->GetLayer()) {
                    continue;
                }
                const auto& layer = spec->GetLayer();
                std::filesystem::path path{layer->GetRealPath()};
                if (path.empty()) {
                    path = layer->GetIdentifier();
                }
                const auto found = source_layer_components_.find(
                    normalized_path_key(path));
                if (found == source_layer_components_.end()) {
                    continue;
                }
                if (found->second == JungleComponent::ROOT) {
                    authored_in_root = true;
                    continue;
                }
                return found->second;
            }
            if (authored_in_root) {
                return JungleComponent::ROOT;
            }
        }
        return JungleComponent::UNKNOWN;
    }

    bool JungleSceneProfile::is_point_component(
        JungleComponent component) noexcept {

        return std::ranges::find(POINT_COMPONENTS, component) !=
            POINT_COMPONENTS.end();
    }

    bool JungleSceneProfile::is_static_component(
        JungleComponent component) noexcept {

        return component == JungleComponent::PYRAMID ||
            component == JungleComponent::RIVER ||
            component == JungleComponent::CREEK ||
            component == JungleComponent::BANYAN ||
            component == JungleComponent::TERRAIN_EXTENDED ||
            component == JungleComponent::TERRAIN_CINEMATIC;
    }

    std::span<const JungleComponent>
    JungleSceneProfile::point_components() noexcept {
        return POINT_COMPONENTS;
    }

    void JungleSceneProfile::set_point_batch_range(
        scene::StaticScene::Components& components,
        JungleComponent component,
        scene::StaticScene::IndexRange range) {

        switch (component) {
        case JungleComponent::ANTHURIUM:
            components.anthurium.point_batches = range;
            break;
        case JungleComponent::NETTLE:
            components.nettle.point_batches = range;
            break;
        case JungleComponent::SHRUB_SORREL:
            components.shrub_sorrel.point_batches = range;
            break;
        case JungleComponent::SHRUB:
            components.shrub.point_batches = range;
            break;
        case JungleComponent::GRASS_B:
            components.grass_b.point_batches = range;
            break;
        case JungleComponent::GRASS_A:
            components.grass_a.point_batches = range;
            break;
        case JungleComponent::PYRAMID_GRASS_B:
            components.pyramid_grass_b.point_batches = range;
            break;
        case JungleComponent::PYRAMID_MOSS:
            components.pyramid_moss.point_batches = range;
            break;
        case JungleComponent::QUEEN_FOREST:
            components.queen_forest.point_batches = range;
            break;
        case JungleComponent::RIVER_FOREST:
            components.river_forest.point_batches = range;
            break;
        case JungleComponent::RIVER_SAPLING:
            components.river_sapling.point_batches = range;
            break;
        case JungleComponent::RIVER_SEEDLING:
            components.river_seedling.point_batches = range;
            break;
        default:
            fail("Non-point component received a point-batch range.");
        }
    }

    void JungleSceneProfile::validate_contract(
        const scene::StaticScene& source) {

        const auto& components = source.components;
        const std::array point_ranges{
            components.anthurium.point_batches,
            components.nettle.point_batches,
            components.shrub_sorrel.point_batches,
            components.shrub.point_batches,
            components.grass_b.point_batches,
            components.grass_a.point_batches,
            components.pyramid_grass_b.point_batches,
            components.pyramid_moss.point_batches,
            components.queen_forest.point_batches,
            components.river_forest.point_batches,
            components.river_sapling.point_batches,
            components.river_seedling.point_batches,
        };

        std::uint32_t expected_offset = 0;
        for (const auto& range : point_ranges) {
            if (range.count == 0 || range.offset != expected_offset) {
                fail("Point components must form non-empty contiguous ranges.");
            }
            expected_offset += range.count;
        }
        if (expected_offset != source.point_batches.size()) {
            fail("Point component ranges do not cover all batches.");
        }
        if (components.terrain.extended.count == 0 ||
            components.terrain.cinematic.count == 0) {
            fail("Both compiler-known Terrain regions must be non-empty.");
        }
        const auto static_count =
            4u + components.terrain.extended.count +
            components.terrain.cinematic.count;
        if (static_count != source.static_mesh_instances.size()) {
            fail("Static components do not cover all static instances.");
        }
        if (source.camera.name == scene::StaticScene::INVALID_INDEX ||
            source.environment_light.name == scene::StaticScene::INVALID_INDEX) {
            fail("Jungle root must provide one camera and environment light.");
        }
    }

} // namespace fjr::cooker::internal
