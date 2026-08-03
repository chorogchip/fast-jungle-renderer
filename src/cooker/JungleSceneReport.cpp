#include "FastJungle/cooker/JungleSceneReport.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "FastJungle/cooker/JungleSceneValidator.hpp"

namespace fjr::cooker {

    namespace {

        using Scene = fjr::cooker::JungleScene;

        struct VegetationSummary {
            Scene::ObjectKind kind;
            std::uint64_t instancers = 0;
            std::uint64_t instances = 0;
        };

        std::vector<VegetationSummary> summarize_vegetation(
            const Scene& scene) {

            using Kind = Scene::ObjectKind;
            std::vector<VegetationSummary> result{
                {Kind::Anthurium},
                {Kind::GrassA},
                {Kind::GrassB},
                {Kind::PyramidGrassB},
                {Kind::PyramidMoss},
                {Kind::QueenForest},
                {Kind::RiverForest},
                {Kind::RiverSapling},
                {Kind::RiverSeedling},
                {Kind::Shrub},
                {Kind::ShrubSorrel},
                {Kind::Nettle},
            };
            for (const auto& instancer : scene.point_instancers) {
                for (auto& summary : result) {
                    if (summary.kind == instancer.object_kind) {
                        ++summary.instancers;
                        summary.instances += instancer.positions.size();
                        break;
                    }
                }
            }
            return result;
        }

        void count_asset(
            const Scene::AssetReference& asset,
            std::uint64_t& assets,
            std::uint64_t& missing_assets) {

            if (asset.authored_path.empty()) {
                return;
            }
            ++assets;
            if (!asset.resolved_file_exists) {
                ++missing_assets;
            }
        }

        int print_report(const Scene& scene) {
            std::uint64_t total_instances = 0;
            for (const auto& instancer : scene.point_instancers) {
                total_instances += instancer.positions.size();
            }

            std::uint64_t asset_references = 0;
            std::uint64_t missing_assets = 0;
            for (const auto& shader : scene.shader_nodes) {
                for (const auto& input : shader.inputs) {
                    if (input.value.kind ==
                        Scene::ShaderValueKind::Asset) {
                        count_asset(
                            std::get<Scene::AssetReference>(
                                input.value.data),
                            asset_references,
                            missing_assets);
                    }
                }
            }
            for (const auto& light : scene.environment_lights) {
                count_asset(
                    light.texture,
                    asset_references,
                    missing_assets);
            }

            std::cout
                << "Intel Jungle Scene imported into project-owned data\n"
                << "  source             : " << scene.source_root << '\n'
                << "  stage              : " << scene.up_axis << "-up, "
                << scene.meters_per_unit << " meters/unit, time "
                << scene.start_time_code << ".." << scene.end_time_code << '\n'
                << "  source layers      : " << scene.source_layers.size() << '\n'
                << "  composed prims     : "
                << scene.statistics.composed_prim_count << '\n'
                << "  owned nodes        : " << scene.nodes.size() << '\n'
                << "  meshes / subsets   : " << scene.meshes.size() << " / "
                << scene.mesh_subsets.size() << " (composed "
                << scene.statistics.composed_mesh_count << " / "
                << scene.statistics.composed_mesh_subset_count << ")\n"
                << "  materials / shaders: " << scene.materials.size() << " / "
                << scene.shader_nodes.size() << '\n'
                << "  cameras / dome     : " << scene.cameras.size() << " / "
                << scene.environment_lights.size() << '\n'
                << "  native instances   : " << scene.native_instances.size()
                << " (" << scene.statistics.native_prototype_count
                << " prototypes)\n"
                << "  point instancers   : " << scene.point_instancers.size()
                << " (" << total_instances << " instances)\n"
                << "  exact-origin points: "
                << scene.statistics.exact_origin_instance_count << '\n'
                << "  time samples       : "
                << scene.statistics.time_sampled_attribute_count
                << " attributes / " << scene.statistics.time_sample_count
                << " samples\n"
                << "  texture references : " << asset_references
                << " (" << missing_assets << " unresolved)\n";

            std::cout << "\nVerified vegetation groups\n";
            for (const auto& summary : summarize_vegetation(scene)) {
                std::cout << "  "
                    << Scene::object_kind_name(summary.kind)
                    << ": " << summary.instancers << " instancers, "
                    << summary.instances << " instances\n";
            }

            auto validation = fjr::cooker::JungleSceneValidator::validate(scene);
            std::uint64_t warnings = 0;
            std::uint64_t errors = 0;
            const auto print_diagnostics = [&warnings, &errors](
                const std::vector<Scene::Diagnostic>& diagnostics) {

                    for (const auto& diagnostic : diagnostics) {
                        const char* severity = "info";
                        if (diagnostic.severity ==
                            Scene::DiagnosticSeverity::Warning) {
                            severity = "warning";
                            ++warnings;
                        } else if (diagnostic.severity ==
                            Scene::DiagnosticSeverity::Error) {
                            severity = "error";
                            ++errors;
                        }
                        std::cout << '[' << severity << "] " << diagnostic.subject
                            << ": " << diagnostic.message << '\n';
                    }
                };
            print_diagnostics(scene.import_diagnostics);
            print_diagnostics(validation);

            std::cout << "\nValidation: " << errors << " errors, "
                << warnings << " warnings\n";
            return errors == 0 ? 0 : 1;
        }

    } // namespace

	void JungleSceneReport::report(const JungleScene& scene) {
	
        print_report(scene);

	}
}