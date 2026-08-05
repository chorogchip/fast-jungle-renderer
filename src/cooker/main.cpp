
#include <cstdlib>
#include <array>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#ifndef FASTJUNGLE_DEFAULT_SCENE_USD
#define FASTJUNGLE_DEFAULT_SCENE_USD ""
#endif

#ifndef FASTJUNGLE_DEFAULT_COOKED_DIR
#define FASTJUNGLE_DEFAULT_COOKED_DIR "."
#endif

#include "FastJungle/cooker/StaticSceneBuilder.hpp"
#include "FastJungle/cooker/MeshLodCooker.hpp"
#include "FastJungle/cooker/TextureCooker.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/ProcessMemory.hpp"
#include "FastJungle/core/util/TemporaryFile.hpp"
#include "FastJungle/scene/StaticSceneReader.hpp"
#include "FastJungle/scene/StaticSceneWriter.hpp"

namespace {

    constexpr wchar_t DEFAULT_SCENE_NAME[] = L"JungleRuins.fjscene";

    void print_usage() {
        std::wcerr
            << L"Usage: FastJungleCooker.exe [input.usd[a|c|z]] "
            << L"[output.fjscene]\n"
            << L"       FastJungleCooker.exe --verify-scene input.fjscene\n"
			<< L"       FastJungleCooker.exe --analyze-lods input.fjscene\n";
    }

    void print_memory_usage(std::wstring_view stage) {
        constexpr double MEBIBYTE = 1024.0 * 1024.0;
        const auto memory = fjr::util::ProcessMemory::query();
        if (!memory) {
            return;
        }

        std::wcout
            << L"  memory after " << stage << L": private "
            << memory->private_bytes / MEBIBYTE
            << L" MiB, working set "
            << memory->working_set_bytes / MEBIBYTE
            << L" MiB, peak working set "
            << memory->peak_working_set_bytes / MEBIBYTE
            << L" MiB\n";
    }

	void print_component_summary(const fjr::scene::StaticScene& scene) {
		auto print_points = [&scene](
			std::wstring_view name,
			fjr::scene::StaticScene::IndexRange range) {

			std::uint64_t instances = 0;
			for (std::uint32_t local = 0; local < range.count; ++local) {
				instances += scene.point_batches[
					static_cast<std::size_t>(range.offset) + local]
					.instance_count;
			}
			std::wcout << L"    " << name << L": "
				<< range.count << L" point batches / "
				<< instances << L" instances\n";
		};

		std::wcout << L"  compiler-known components:\n";
		std::wcout << L"    Pyramid, River, Creek, Banyan: one static instance each\n";
		std::wcout << L"    Terrain: "
			<< scene.components.terrain.extended.count << L" extended / "
			<< scene.components.terrain.cinematic.count << L" cinematic\n";
		print_points(L"Anthurium", scene.components.anthurium.point_batches);
		print_points(L"Nettle", scene.components.nettle.point_batches);
		print_points(L"ShrubSorrel", scene.components.shrub_sorrel.point_batches);
		print_points(L"Shrub", scene.components.shrub.point_batches);
		print_points(L"Grass_B", scene.components.grass_b.point_batches);
		print_points(L"Grass_A", scene.components.grass_a.point_batches);
		print_points(L"Pyramid_Grass_B", scene.components.pyramid_grass_b.point_batches);
		print_points(L"Pyramid_Moss", scene.components.pyramid_moss.point_batches);
		print_points(L"QueenForest", scene.components.queen_forest.point_batches);
		print_points(L"RiverForest", scene.components.river_forest.point_batches);
		print_points(L"RiverSapling", scene.components.river_sapling.point_batches);
		print_points(L"RiverSeedling", scene.components.river_seedling.point_batches);
    }

    void print_scene_summary(const fjr::scene::StaticScene& scene) {
        constexpr std::uint64_t OLD_VERTEX_SIZE = 48;
        constexpr std::uint64_t MEBIBYTE = 1024 * 1024;
        const auto before = scene.info.vertex_count_before_indexing;
        const auto after = scene.info.vertex_count_after_indexing;
        const double reduction = before == 0
            ? 0.0
            : 100.0 * static_cast<double>(before - after) /
                static_cast<double>(before);
        const auto old_unindexed_bytes = before * OLD_VERTEX_SIZE;
        const auto no_tangent_unindexed_bytes =
            before * sizeof(fjr::scene::StaticScene::Vertex);
        const auto indexed_no_tangent_bytes =
            after * sizeof(fjr::scene::StaticScene::Vertex);
		std::array<std::uint64_t, 4> lod_index_counts{};
		bool has_four_lods = !scene.meshes.empty();
		for (const auto& mesh : scene.meshes) {
			has_four_lods &= mesh.lod_count ==
				static_cast<std::uint32_t>(lod_index_counts.size());
			for (std::uint32_t local_lod = 0;
				 local_lod < mesh.lod_count &&
				 local_lod < static_cast<std::uint32_t>(lod_index_counts.size());
				 ++local_lod) {
				const auto& lod = scene.mesh_lods[mesh.lod_offset + local_lod];
				for (std::uint32_t local_submesh = 0;
					 local_submesh < lod.submesh_count;
					 ++local_submesh) {
					lod_index_counts[local_lod] += scene.submeshes[
						lod.submesh_offset + local_submesh].index_count;
				}
			}
		}

        std::wcout
            << L"  vertices before indexing: " << before << L'\n'
            << L"  vertices after indexing: " << after << L'\n'
            << L"  indexing reduction: " << std::fixed
            << std::setprecision(2) << reduction << L"%\n"
            << L"  vertex memory (tangent, unindexed): "
            << old_unindexed_bytes / static_cast<double>(MEBIBYTE)
            << L" MiB\n"
            << L"  vertex memory (no tangent, unindexed): "
            << no_tangent_unindexed_bytes / static_cast<double>(MEBIBYTE)
            << L" MiB\n"
            << L"  vertex memory (no tangent, indexed): "
            << indexed_no_tangent_bytes / static_cast<double>(MEBIBYTE)
            << L" MiB\n"
            << L"  indices: " << scene.indices.size() << L'\n'
            << L"  textures: " << scene.textures.size() << L'\n'
            << L"  materials: " << scene.materials.size() << L'\n'
			<< L"  meshes: " << scene.meshes.size() << L'\n'
			<< L"  instanced mesh definitions: "
			<< scene.instanced_mesh_definitions.size() << L'\n'
			<< L"  point batches: " << scene.point_batches.size() << L'\n'
			<< L"  point instances: " << scene.point_instances.size() << L'\n'
			<< L"  static mesh instances: "
			<< scene.static_mesh_instances.size() << L'\n';
		if (has_four_lods) {
			std::wcout
				<< L"  LOD logical indices (100/40/15/4): "
				<< lod_index_counts[0] << L" / "
				<< lod_index_counts[1] << L" / "
				<< lod_index_counts[2] << L" / "
				<< lod_index_counts[3] << L'\n';
		}
		print_component_summary(scene);
    }

    [[nodiscard]] int run_cooker(int argc, wchar_t** argv) {
        if (argc >= 2 && std::wstring_view{argv[1]} == L"--verify-scene") {
            if (argc != 3) {
                print_usage();
                return EXIT_FAILURE;
            }

            const std::filesystem::path input_path{argv[2]};
            const auto metadata =
                fjr::scene::StaticSceneReader::load_metadata(input_path);
            std::wcout << L"StaticScene read and validated: "
                       << input_path << L'\n';
            print_scene_summary(*metadata.scene);
            std::wcout
                << L"  texture payload: "
                << metadata.texture_payload.size /
                    static_cast<double>(1024 * 1024)
                << L" MiB\n";
            return EXIT_SUCCESS;
        }
		if (argc >= 2 && std::wstring_view{argv[1]} == L"--analyze-lods") {
			if (argc != 3) {
				print_usage();
				return EXIT_FAILURE;
			}

			const std::filesystem::path input_path{argv[2]};
			auto metadata =
				fjr::scene::StaticSceneReader::load_metadata(input_path);
			const auto stats = fjr::cooker::MeshLodCooker::cook(
				*metadata.scene);
			std::wcout
				<< L"LOD analysis complete: " << input_path << L'\n'
				<< L"  generated index storage: "
				<< stats.generated_index_count << L" indices\n"
				<< L"  simplified/reused submesh levels: "
				<< stats.simplified_submeshes << L" / "
				<< stats.reused_submeshes << L'\n'
				<< L"  sloppy fallback submesh levels: "
				<< stats.sloppy_fallback_submeshes << L'\n';
			print_scene_summary(*metadata.scene);
			return EXIT_SUCCESS;
		}

        if (argc > 3) {
            print_usage();
            return EXIT_FAILURE;
        }

        const std::filesystem::path root_layer = argc >= 2
            ? std::filesystem::path{argv[1]}
            : std::filesystem::path{FASTJUNGLE_DEFAULT_SCENE_USD};

        const std::filesystem::path output_path = argc >= 3
            ? std::filesystem::path{argv[2]}
            : std::filesystem::path{FASTJUNGLE_DEFAULT_COOKED_DIR} /
                DEFAULT_SCENE_NAME;

        if (root_layer.empty()) {
            fjr::log::Logger::g_logger
                << "No input USD layer was supplied and no default is configured.";
            fjr::log::Logger::g_logger.abort();
        }
        if (!std::filesystem::is_regular_file(root_layer)) {
            fjr::log::Logger::g_logger
                << "Input USD layer does not exist: "
                << root_layer.generic_string();
            fjr::log::Logger::g_logger.abort();
        }

        if (const auto parent = output_path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        std::wcout
            << L"Building scene from " << root_layer << L"...\n"
            << std::flush;
        auto build = fjr::cooker::StaticSceneBuilder::build(root_layer);
        auto scene = std::move(build.scene);

        std::wcout
            << L"StaticScene metadata built; OpenUSD stage released\n";
        print_memory_usage(L"scene metadata");

        std::wcout
            << L"Cooking mesh LODs at 100% / 40% / 15% / 4%...\n"
            << std::flush;
        const auto lod_stats = fjr::cooker::MeshLodCooker::cook(*scene);
        std::wcout
            << L"Mesh LOD logical index counts: "
            << lod_stats.logical_index_counts[0] << L" / "
            << lod_stats.logical_index_counts[1] << L" / "
            << lod_stats.logical_index_counts[2] << L" / "
            << lod_stats.logical_index_counts[3] << L'\n'
            << L"  generated index storage: "
            << lod_stats.generated_index_count << L" indices\n"
            << L"  simplified/reused submesh levels: "
            << lod_stats.simplified_submeshes << L" / "
            << lod_stats.reused_submeshes << L'\n'
            << L"  sloppy fallback submesh levels: "
            << lod_stats.sloppy_fallback_submeshes << L'\n';
        print_memory_usage(L"mesh LOD cook");
        print_scene_summary(*scene);
        std::wcout << std::flush;

        auto texture_payload_path = output_path;
        texture_payload_path += L".textures.tmp";
        fjr::util::TemporaryFile texture_payload{
            std::move(texture_payload_path)
        };

        std::wcout
            << L"Cooking textures sequentially...\n"
            << std::flush;
        const std::uint64_t texture_payload_size =
            fjr::cooker::TextureCooker::cook(
                *scene,
                build.texture_paths,
                texture_payload.path());
        std::wcout
            << L"Texture payload cooked: "
            << texture_payload_size / static_cast<double>(1024 * 1024)
            << L" MiB\n"
            << std::flush;
        print_memory_usage(L"texture cook");

        fjr::scene::StaticSceneWriter::save(
            output_path,
            *scene,
            texture_payload.path(),
            texture_payload_size);
        texture_payload.remove();
        std::wcout
            << L"Saved " << output_path << L'\n'
            << L"Saved "
            << fjr::scene::StaticSceneWriter::texture_path(output_path)
            << L'\n';
        print_memory_usage(L"scene save");
        return EXIT_SUCCESS;
    }

} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        return run_cooker(argc, argv);
    }
    catch (const std::exception& exception) {
        fjr::log::Logger::g_logger
            << "FastJungleCooker: " << exception.what();
        fjr::log::Logger::g_logger.abort();
    }
}
