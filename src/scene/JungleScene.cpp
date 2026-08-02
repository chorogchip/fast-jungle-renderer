#include "FastJungle/scene/JungleScene.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <string_view>
#include <unordered_set>

namespace fjr::scene {

    namespace {

        bool starts_with_path(
            const std::string& path,
            std::string_view prefix) {

            return path.starts_with(prefix);
        }

        void add_error(
            std::vector<SceneDiagnostic>& diagnostics,
            const std::string& subject,
            const std::string& message) {

            diagnostics.push_back({
                DiagnosticSeverity::Error,
                subject,
                message
            });
        }

        void validate_asset_reference(
            std::vector<SceneDiagnostic>& diagnostics,
            const std::string& subject,
            const AssetReference& asset) {

            if (!asset.authored_path.empty() &&
                !asset.resolved_file_exists) {
                add_error(
                    diagnostics,
                    subject,
                    "Authored asset does not resolve to a source file: " +
                        asset.authored_path);
            }
        }

        std::uint64_t instance_count(
            const JungleScene& scene,
            JungleObjectKind kind) {

            std::uint64_t count = 0;
            for (const auto& instancer : scene.point_instancers) {
                if (instancer.object_kind == kind) {
                    count += instancer.positions.size();
                }
            }
            return count;
        }

        std::uint64_t instancer_count(
            const JungleScene& scene,
            JungleObjectKind kind) {

            return static_cast<std::uint64_t>(std::count_if(
                scene.point_instancers.begin(),
                scene.point_instancers.end(),
                [kind](const PointInstancer& instancer) {
                    return instancer.object_kind == kind;
                }));
        }

        std::string prototype_name(const std::string& path) {
            const auto separator = path.find_last_of('/');
            return separator == std::string::npos
                ? path
                : path.substr(separator + 1);
        }

    } // namespace

    JungleObjectKind classify_jungle_object(const std::string& prim_path) {
        struct Rule {
            std::string_view prefix;
            JungleObjectKind kind;
        };

        // The order is significant where one verified Jungle name is a prefix
        // of another (Shrub/ShrubSorrel and Grass/Pyramid_Grass).
        static constexpr std::array rules{
            Rule{"/World/Pyramid_Grass_B_", JungleObjectKind::PyramidGrassB},
            Rule{"/World/Anthurium_", JungleObjectKind::Anthurium},
            Rule{"/World/Grass_A_", JungleObjectKind::GrassA},
            Rule{"/World/Grass_B_", JungleObjectKind::GrassB},
            Rule{"/World/Moss_", JungleObjectKind::PyramidMoss},
            Rule{"/World/QueenForest_", JungleObjectKind::QueenForest},
            Rule{"/World/RiverForest_", JungleObjectKind::RiverForest},
            Rule{"/World/RiverSapling_", JungleObjectKind::RiverSapling},
            Rule{"/World/RiverSeedling_", JungleObjectKind::RiverSeedling},
            Rule{"/World/ShrubSorrel_", JungleObjectKind::ShrubSorrel},
            Rule{"/World/Shrub_", JungleObjectKind::Shrub},
            Rule{"/World/Nettle_", JungleObjectKind::Nettle},
            Rule{"/root/", JungleObjectKind::Terrain},
            Rule{"/World/Pyramid", JungleObjectKind::Pyramid},
            Rule{"/World/Banyan", JungleObjectKind::Banyan},
            Rule{"/World/River", JungleObjectKind::River},
            Rule{"/World/Creek", JungleObjectKind::Creek},
            Rule{"/Environment", JungleObjectKind::Environment},
        };

        for (const auto& rule : rules) {
            if (starts_with_path(prim_path, rule.prefix)) {
                return rule.kind;
            }
        }
        if (prim_path.find("/Camera") != std::string::npos) {
            return JungleObjectKind::Camera;
        }
        return JungleObjectKind::Unknown;
    }

    const char* jungle_object_kind_name(JungleObjectKind kind) {
        switch (kind) {
        case JungleObjectKind::Unknown: return "Unknown";
        case JungleObjectKind::Anthurium: return "Anthurium";
        case JungleObjectKind::GrassA: return "Grass A";
        case JungleObjectKind::GrassB: return "Grass B";
        case JungleObjectKind::PyramidGrassB: return "Pyramid Grass B";
        case JungleObjectKind::PyramidMoss: return "Pyramid Moss";
        case JungleObjectKind::QueenForest: return "Queen Forest";
        case JungleObjectKind::RiverForest: return "River Forest";
        case JungleObjectKind::RiverSapling: return "River Sapling";
        case JungleObjectKind::RiverSeedling: return "River Seedling";
        case JungleObjectKind::Shrub: return "Shrub";
        case JungleObjectKind::ShrubSorrel: return "Shrub Sorrel";
        case JungleObjectKind::Nettle: return "Nettle";
        case JungleObjectKind::Terrain: return "Terrain";
        case JungleObjectKind::Pyramid: return "Pyramid";
        case JungleObjectKind::Banyan: return "Banyan";
        case JungleObjectKind::River: return "River";
        case JungleObjectKind::Creek: return "Creek";
        case JungleObjectKind::Environment: return "Environment";
        case JungleObjectKind::Camera: return "Camera";
        }
        return "Unknown";
    }

    std::vector<SceneDiagnostic> validate_jungle_scene(
        const JungleScene& scene) {

        std::vector<SceneDiagnostic> diagnostics;

        if (scene.up_axis != "Z") {
            add_error(diagnostics, "stage", "Intel Jungle must be Z-up.");
        }
        if (std::abs(scene.meters_per_unit - 0.01) > 1.0e-9) {
            add_error(
                diagnostics,
                "stage",
                "Intel Jungle metersPerUnit must be 0.01.");
        }

        std::unordered_set<std::string> node_paths;
        node_paths.reserve(scene.nodes.size());
        for (const auto& node : scene.nodes) {
            node_paths.insert(node.path);
        }
        for (const std::string_view root : {
            "/root",
            "/World",
            "/Environment"}) {
            if (!node_paths.contains(std::string{root})) {
                add_error(
                    diagnostics,
                    "stage",
                    "Missing verified root prim: " + std::string{root});
            }
        }

        std::unordered_set<std::string> material_paths;
        material_paths.reserve(scene.materials.size());
        for (const auto& material : scene.materials) {
            material_paths.insert(material.prim_path);
        }

        for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
            const auto& node = scene.nodes[i];
            if (node.parent != INVALID_SCENE_INDEX &&
                node.parent >= scene.nodes.size()) {
                add_error(diagnostics, node.path, "Invalid parent index.");
            }

            std::size_t payload_count = 0;
            switch (node.prim_kind) {
            case PrimKind::Mesh:
                payload_count = scene.meshes.size();
                break;
            case PrimKind::GeomSubset:
                payload_count = scene.mesh_subsets.size();
                break;
            case PrimKind::PointInstancer:
                payload_count = scene.point_instancers.size();
                break;
            case PrimKind::Material:
                payload_count = scene.materials.size();
                break;
            case PrimKind::Shader:
                payload_count = scene.shader_nodes.size();
                break;
            case PrimKind::Camera:
                payload_count = scene.cameras.size();
                break;
            case PrimKind::Light:
                payload_count = scene.environment_lights.size();
                break;
            default:
                continue;
            }
            if (node.payload == INVALID_SCENE_INDEX ||
                node.payload >= payload_count) {
                add_error(diagnostics, node.path, "Invalid typed payload index.");
            }
        }

        std::uint64_t total_instances = 0;
        std::unordered_set<std::string> prototype_names;
        for (const auto& instancer : scene.point_instancers) {
            const auto count = instancer.prototype_indices.size();
            total_instances += count;

            if (instancer.object_kind == JungleObjectKind::Unknown) {
                add_error(
                    diagnostics,
                    instancer.prim_path,
                    "PointInstancer has no verified Jungle object kind.");
            }
            if (instancer.prototype_paths.size() != 1) {
                add_error(
                    diagnostics,
                    instancer.prim_path,
                    "Intel Jungle PointInstancer must target one prototype.");
            }
            if (!instancer.prototype_paths.empty()) {
                prototype_names.insert(
                    prototype_name(instancer.prototype_paths.front()));
            }
            for (const auto& prototype_path : instancer.prototype_paths) {
                if (!node_paths.contains(prototype_path)) {
                    add_error(
                        diagnostics,
                        instancer.prim_path,
                        "Point prototype is absent from the imported hierarchy: " +
                            prototype_path);
                }
            }
            if (instancer.positions.size() != count ||
                instancer.orientations.size() != count ||
                instancer.scales.size() != count) {
                add_error(
                    diagnostics,
                    instancer.prim_path,
                    "PointInstancer TRS array sizes do not match protoIndices.");
            }
            if (std::any_of(
                instancer.prototype_indices.begin(),
                instancer.prototype_indices.end(),
                [&instancer](std::int32_t index) {
                    return index < 0 ||
                        static_cast<std::size_t>(index) >=
                            instancer.prototype_paths.size();
                })) {
                add_error(
                    diagnostics,
                    instancer.prim_path,
                    "PointInstancer contains an invalid prototype index.");
            }
        }

        for (const auto& mesh : scene.meshes) {
            const auto expected_index_count = std::accumulate(
                mesh.face_vertex_counts.begin(),
                mesh.face_vertex_counts.end(),
                std::int64_t{0});
            if (expected_index_count !=
                static_cast<std::int64_t>(mesh.face_vertex_indices.size())) {
                add_error(
                    diagnostics,
                    mesh.prim_path,
                    "Mesh face counts do not match the index count.");
            }
            if (std::any_of(
                mesh.face_vertex_indices.begin(),
                mesh.face_vertex_indices.end(),
                [&mesh](std::int32_t index) {
                    return index < 0 ||
                        static_cast<std::size_t>(index) >= mesh.points.size();
                })) {
                add_error(
                    diagnostics,
                    mesh.prim_path,
                    "Mesh contains an out-of-range point index.");
            }
            if (!mesh.material_path.empty() &&
                !material_paths.contains(mesh.material_path)) {
                add_error(
                    diagnostics,
                    mesh.prim_path,
                    "Bound material is absent from imported materials: " +
                        mesh.material_path);
            }
        }

        for (const auto& subset : scene.mesh_subsets) {
            if (!subset.material_path.empty() &&
                !material_paths.contains(subset.material_path)) {
                add_error(
                    diagnostics,
                    subset.prim_path,
                    "Subset material is absent from imported materials: " +
                        subset.material_path);
            }
        }

        for (const auto& instance : scene.native_instances) {
            if (!node_paths.contains(instance.prototype_path)) {
                add_error(
                    diagnostics,
                    instance.prim_path,
                    "Native prototype is absent from the imported hierarchy: " +
                        instance.prototype_path);
            }
        }

        for (const auto& material : scene.materials) {
            for (const auto shader_index : material.shader_nodes) {
                if (shader_index >= scene.shader_nodes.size()) {
                    add_error(
                        diagnostics,
                        material.prim_path,
                        "Material contains an invalid shader-node index.");
                }
            }
        }

        for (const auto& shader : scene.shader_nodes) {
            for (const auto& input : shader.inputs) {
                if (input.value.kind == ShaderValueKind::Asset) {
                    validate_asset_reference(
                        diagnostics,
                        shader.prim_path + ".inputs:" + input.name,
                        std::get<AssetReference>(input.value.data));
                }
            }
        }
        for (const auto& light : scene.environment_lights) {
            validate_asset_reference(
                diagnostics,
                light.prim_path,
                light.texture);
        }

        struct ExpectedVegetation {
            JungleObjectKind kind;
            std::uint64_t instancers;
            std::uint64_t instances;
        };
        static constexpr std::array expected_vegetation{
            ExpectedVegetation{JungleObjectKind::Anthurium, 6, 138},
            ExpectedVegetation{JungleObjectKind::GrassA, 6, 280985},
            ExpectedVegetation{JungleObjectKind::GrassB, 5, 339865},
            ExpectedVegetation{JungleObjectKind::PyramidGrassB, 5, 44000},
            ExpectedVegetation{JungleObjectKind::PyramidMoss, 138, 2034610},
            ExpectedVegetation{JungleObjectKind::QueenForest, 195, 613806},
            ExpectedVegetation{JungleObjectKind::RiverForest, 195, 2407967},
            ExpectedVegetation{JungleObjectKind::RiverSapling, 5, 45000},
            ExpectedVegetation{JungleObjectKind::RiverSeedling, 80, 2266462},
            ExpectedVegetation{JungleObjectKind::Shrub, 4, 11337},
            ExpectedVegetation{JungleObjectKind::ShrubSorrel, 133, 630176},
            ExpectedVegetation{JungleObjectKind::Nettle, 6, 330},
        };

        for (const auto& expected : expected_vegetation) {
            if (instancer_count(scene, expected.kind) != expected.instancers ||
                instance_count(scene, expected.kind) != expected.instances) {
                add_error(
                    diagnostics,
                    jungle_object_kind_name(expected.kind),
                    "Instancer or instance count differs from the verified "
                    "Intel Jungle scene.");
            }
        }

        if (scene.point_instancers.size() != 778 ||
            total_instances != 8674676) {
            add_error(
                diagnostics,
                "point instancers",
                "Scene does not match the verified 778 / 8,674,676 signature.");
        }
        if (prototype_names.size() != 53) {
            add_error(
                diagnostics,
                "point prototypes",
                "Scene does not contain the verified 53 prototype names.");
        }
        if (scene.statistics.exact_origin_instance_count != 197) {
            add_error(
                diagnostics,
                "point instances",
                "Exact-origin instance count differs from the verified 197.");
        }
        if (scene.statistics.composed_mesh_count != 121 ||
            scene.statistics.composed_mesh_subset_count != 37 ||
            scene.statistics.composed_material_count != 134 ||
            scene.statistics.composed_shader_count != 674 ||
            scene.statistics.composed_camera_count != 1) {
            add_error(
                diagnostics,
                "composed stage",
                "Mesh/subset/material/shader/camera counts differ from the "
                "verified 121 / 37 / 134 / 674 / 1 signature.");
        }
        if (scene.statistics.composed_prim_count != 3429 ||
            scene.source_layers.size() != 33 ||
            scene.environment_lights.size() != 1) {
            add_error(
                diagnostics,
                "stage inventory",
                "Prim/layer/dome-light counts differ from the verified "
                "3,429 / 33 / 1 signature.");
        }
        if (scene.source_layers.size() != 33 ||
            scene.statistics.composed_prim_count != 3429 ||
            scene.nodes.size() != 3854) {
            add_error(
                diagnostics,
                "composed hierarchy",
                "Layer/prim/owned-node counts differ from the verified "
                "33 / 3,429 / 3,854 signature.");
        }
        if (scene.meshes.size() != 142 ||
            scene.mesh_subsets.size() != 85 ||
            scene.materials.size() != 192 ||
            scene.shader_nodes.size() != 930 ||
            scene.cameras.size() != 1 ||
            scene.environment_lights.size() != 1) {
            add_error(
                diagnostics,
                "owned payloads",
                "Owned mesh/subset/material/shader/camera/light counts differ "
                "from the verified 142 / 85 / 192 / 930 / 1 / 1 signature.");
        }
        if (scene.native_instances.size() != 741 ||
            scene.statistics.native_prototype_count != 21) {
            add_error(
                diagnostics,
                "native instances",
                "Native instance/prototype counts differ from 741 / 21.");
        }

        return diagnostics;
    }

} // namespace fjr::scene
