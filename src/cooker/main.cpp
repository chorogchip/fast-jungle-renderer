
#include <cstdlib>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
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
#include "FastJungle/cooker/ImpostorCooker.hpp"
#include "FastJungle/cooker/MeshLodCooker.hpp"
#include "FastJungle/cooker/TextureCooker.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/ProcessMemory.hpp"
#include "FastJungle/core/util/TemporaryFile.hpp"
#include "FastJungle/scene/StaticSceneReader.hpp"
#include "FastJungle/scene/StaticSceneWriter.hpp"

namespace {

    constexpr wchar_t DEFAULT_SCENE_NAME[] = L"JungleRuins.fjscene";
    constexpr std::array<char, 8> SCENE_MAGIC{
        'F', 'J', 'S', 'C', 'E', 'N', 'E', '\0'
    };
    constexpr std::array<char, 8> TEXTURE_MAGIC{
        'F', 'J', 'T', 'E', 'X', '\0', '\0', '\0'
    };
    // Incremented because mesh LOD contents are part of the cooked scene
    // payload. The cache check otherwise sees a valid binary header and
    // reuses a scene cooked with the previous LOD recipe.
    constexpr std::uint32_t SCENE_VERSION = 11;
    constexpr std::uint32_t TEXTURE_VERSION = 5;

    struct CookedFileHeader final {
        std::array<char, 8> magic{};
        std::uint32_t version = 0;
    };

    [[nodiscard]]
    bool has_current_header(
        const std::filesystem::path& path,
        const std::array<char, 8>& magic,
        std::uint32_t version) {

        std::ifstream input{path, std::ios::binary};
        CookedFileHeader header;
        input.read(
            reinterpret_cast<char*>(&header),
            sizeof(header));
        return input && header.magic == magic && header.version == version;
    }

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

        const auto texture_path =
            fjr::scene::StaticSceneWriter::texture_path(output_path);
        const bool has_scene = has_current_header(
            output_path,
            SCENE_MAGIC,
            SCENE_VERSION);
        const bool has_textures = has_current_header(
            texture_path,
            TEXTURE_MAGIC,
            TEXTURE_VERSION);
        if (has_scene && has_textures) {
            std::wcout
                << L"Reusing " << output_path << L'\n'
                << L"Reusing " << texture_path << L'\n';
            return EXIT_SUCCESS;
        }

        std::wcout
            << L"Building scene from " << root_layer << L"...\n"
            << std::flush;
        auto build = fjr::cooker::StaticSceneBuilder::build(root_layer);
        auto scene = std::move(build.scene);

        std::wcout
            << L"StaticScene metadata built; OpenUSD stage released\n";
        print_memory_usage(L"scene metadata");

        std::wcout << L"Cooking mesh LODs...\n" << std::flush;
        [[maybe_unused]] const auto lod_stats =
            fjr::cooker::MeshLodCooker::cook(*scene);
        print_memory_usage(L"mesh LOD cook");

        std::wcout
            << L"Baking 8-direction impostors for the four highest-cost "
            << L"final-LOD foliage meshes...\n"
            << std::flush;
        auto impostor_result = fjr::cooker::ImpostorCooker::cook(
            *scene,
            !has_textures);
        std::wcout << std::flush;

        if (has_textures) {
            std::wcout
                << L"Reusing cooked textures...\n"
                << std::flush;
            const auto texture_payload_size =
                fjr::cooker::TextureCooker::reuse(
                    *scene,
                    texture_path);
            fjr::scene::StaticSceneWriter::save_metadata(
                output_path,
                *scene,
                texture_payload_size);
            std::wcout
                << L"Saved " << output_path << L'\n'
                << L"Reused " << texture_path << L'\n';
        }
        else {
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
                    texture_payload.path(),
                    {},
                    impostor_result.generated_textures);
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
                << L"Saved " << texture_path << L'\n';
        }
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
