#include "FastJungle/scene/JungleScene.hpp"

#include "FastJungle/core/util/Path.hpp"

#include <array>
#include <string_view>

namespace fjr::scene {

    JungleScene::ObjectKind JungleScene::classify_object(
        const std::string& prim_path) {

        struct Rule {
            std::string_view prefix;
            ObjectKind kind;
        };

        // The order is significant where one verified Jungle name is a prefix
        // of another (Shrub/ShrubSorrel and Grass/Pyramid_Grass).
        static constexpr std::array rules{
            Rule{"/World/Pyramid_Grass_B_", ObjectKind::PyramidGrassB},
            Rule{"/World/Anthurium_", ObjectKind::Anthurium},
            Rule{"/World/Grass_A_", ObjectKind::GrassA},
            Rule{"/World/Grass_B_", ObjectKind::GrassB},
            Rule{"/World/Moss_", ObjectKind::PyramidMoss},
            Rule{"/World/QueenForest_", ObjectKind::QueenForest},
            Rule{"/World/RiverForest_", ObjectKind::RiverForest},
            Rule{"/World/RiverSapling_", ObjectKind::RiverSapling},
            Rule{"/World/RiverSeedling_", ObjectKind::RiverSeedling},
            Rule{"/World/ShrubSorrel_", ObjectKind::ShrubSorrel},
            Rule{"/World/Shrub_", ObjectKind::Shrub},
            Rule{"/World/Nettle_", ObjectKind::Nettle},
            Rule{"/root/", ObjectKind::Terrain},
            Rule{"/World/Pyramid", ObjectKind::Pyramid},
            Rule{"/World/Banyan", ObjectKind::Banyan},
            Rule{"/World/River", ObjectKind::River},
            Rule{"/World/Creek", ObjectKind::Creek},
            Rule{"/Environment", ObjectKind::Environment},
        };

        const util::Path path{prim_path};
        for (const auto& rule : rules) {
            if (path.starts_with(rule.prefix)) {
                return rule.kind;
            }
        }
        if (prim_path.find("/Camera") != std::string::npos) {
            return ObjectKind::Camera;
        }
        return ObjectKind::Unknown;
    }

    const char* JungleScene::object_kind_name(
        ObjectKind kind) noexcept {

        switch (kind) {
        case ObjectKind::Unknown: return "Unknown";
        case ObjectKind::Anthurium: return "Anthurium";
        case ObjectKind::GrassA: return "Grass A";
        case ObjectKind::GrassB: return "Grass B";
        case ObjectKind::PyramidGrassB: return "Pyramid Grass B";
        case ObjectKind::PyramidMoss: return "Pyramid Moss";
        case ObjectKind::QueenForest: return "Queen Forest";
        case ObjectKind::RiverForest: return "River Forest";
        case ObjectKind::RiverSapling: return "River Sapling";
        case ObjectKind::RiverSeedling: return "River Seedling";
        case ObjectKind::Shrub: return "Shrub";
        case ObjectKind::ShrubSorrel: return "Shrub Sorrel";
        case ObjectKind::Nettle: return "Nettle";
        case ObjectKind::Terrain: return "Terrain";
        case ObjectKind::Pyramid: return "Pyramid";
        case ObjectKind::Banyan: return "Banyan";
        case ObjectKind::River: return "River";
        case ObjectKind::Creek: return "Creek";
        case ObjectKind::Environment: return "Environment";
        case ObjectKind::Camera: return "Camera";
        }
        return "Unknown";
    }

} // namespace fjr::scene
