#include "JungleSceneProfile.hpp"

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/Path.hpp"

#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <ranges>
#include <set>
#include <string_view>

namespace fjr::cooker::internal {

    using log::fail;

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
            util::Path{root_path}.normalized_key(),
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
                util::Path{real_path}.normalized_key(),
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
                    util::Path{path}.normalized_key());
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

    scene::StaticScene::EnumPointCategory
    JungleSceneProfile::point_category(JungleComponent component) {

        using Category = scene::StaticScene::EnumPointCategory;

        switch (component) {
        case JungleComponent::ANTHURIUM:
            return Category::ANTHURIUM;
        case JungleComponent::NETTLE:
            return Category::NETTLE;
        case JungleComponent::SHRUB_SORREL:
            return Category::SHRUB_SORREL;
        case JungleComponent::SHRUB:
            return Category::SHRUB;
        case JungleComponent::GRASS_B:
            return Category::GRASS_B;
        case JungleComponent::GRASS_A:
            return Category::GRASS_A;
        case JungleComponent::PYRAMID_GRASS_B:
            return Category::PYRAMID_GRASS_B;
        case JungleComponent::PYRAMID_MOSS:
            return Category::PYRAMID_MOSS;
        case JungleComponent::QUEEN_FOREST:
            return Category::QUEEN_FOREST;
        case JungleComponent::RIVER_FOREST:
            return Category::RIVER_FOREST;
        case JungleComponent::RIVER_SAPLING:
            return Category::RIVER_SAPLING;
        case JungleComponent::RIVER_SEEDLING:
            return Category::RIVER_SEEDLING;
        default:
            fail("Non-point component received as a point category.");
        }
    }

} // namespace fjr::cooker::internal
