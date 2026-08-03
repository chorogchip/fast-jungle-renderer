
#include <cstdlib>
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
            << L"       FastJungleCooker.exe --verify-scene input.fjscene\n";
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
            << L"  prototypes: " << scene.prototypes.size() << L'\n'
            << L"  point batches: " << scene.point_batches.size() << L'\n'
            << L"  point instances: " << scene.point_instances.size() << L'\n'
            << L"  matrix batches: " << scene.matrix_batches.size() << L'\n'
            << L"  matrix instances: " << scene.matrix_instances.size() << L'\n';
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
        std::wcout << L"Saved " << output_path << L'\n';
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
