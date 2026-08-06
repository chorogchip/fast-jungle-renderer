#include "FastJungle/scene/StaticSceneReader.hpp"

#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

#include "FastJungle/scene/StaticSceneFileIO.hpp"

#include <limits>
#include <utility>

namespace fjr::scene {

    namespace {

        [[nodiscard]]
        std::unique_ptr<StaticScene> allocate_scene(
            const std::filesystem::path& path) {

            try {
                return std::make_unique<StaticScene>();
            }
            catch (...) {
                log::Logger::g_logger
                    << "Failed to allocate StaticScene: " << path << '\n';
                log::Logger::g_logger.abort();
            }
        }

    } // namespace

    std::unique_ptr<StaticScene> StaticSceneReader::load(
        const std::filesystem::path& path) {

        auto metadata = load_metadata(path);
        auto& scene = metadata.scene;
        if (metadata.texture_payload.size >
            std::numeric_limits<std::size_t>::max()) {
            log::Logger::g_logger
                << "StaticTexture payload exceeds addressable memory: "
                << metadata.texture_payload.path << '\n';
            log::Logger::g_logger.abort();
        }

        auto texture_source = util::File::open_read(
            metadata.texture_payload.path);
        static_scene_file_io::Reader texture_reader{
            texture_source,
            util::File::size(metadata.texture_payload.path),
            metadata.texture_payload.path
        };
        const auto texture_size =
            static_scene_file_io::read_texture_header(texture_reader);
        texture_reader.skip(texture_size.metadata_size);
        try {
            scene->texture_data.resize(
                static_cast<std::size_t>(texture_size.payload_size));
        }
        catch (...) {
            log::Logger::g_logger
                << "Failed to allocate StaticTexture payload: "
                << metadata.texture_payload.path << '\n';
            log::Logger::g_logger.abort();
        }
        texture_reader.read_raw(
            scene->texture_data.data(),
            scene->texture_data.size());
        texture_reader.require_end();
        StaticSceneValidator::validate(*scene);
        return std::move(scene);
    }

    StaticSceneMetadata StaticSceneReader::load_metadata(
        const std::filesystem::path& path) {

        auto source = util::File::open_read(path);
        static_scene_file_io::Reader reader{
            source,
            util::File::size(path),
            path
        };

        const auto expected_texture_size =
            static_scene_file_io::read_header(reader);

        StaticSceneMetadata result;
        result.scene = allocate_scene(path);

#define X(type, name) reader.read(result.scene->name);
        SceneData_MACRO
#undef X

        reader.read(result.scene->components);
        reader.read(result.scene->camera);
        reader.read(result.scene->environment_light);
        reader.read(result.scene->info);
        reader.require_end();

        const auto external_path = texture_path(path);
        auto texture_source = util::File::open_read(external_path);
        static_scene_file_io::Reader texture_reader{
            texture_source,
            util::File::size(external_path),
            external_path
        };
        const auto actual_texture_size =
            static_scene_file_io::read_texture_header(texture_reader);
        if (actual_texture_size.payload_size != expected_texture_size) {
            log::Logger::g_logger
                << "StaticScene and StaticTexture payload sizes differ.\n"
                << "  scene: " << path << '\n'
                << "  texture: " << external_path << '\n';
            log::Logger::g_logger.abort();
        }

        result.texture_payload = {
            .path = external_path,
            .file_offset = static_scene_file_io::texture_header_size() +
                actual_texture_size.metadata_size,
            .size = actual_texture_size.payload_size
        };
        StaticSceneValidator::validate(
            *result.scene,
            result.texture_payload.size);
        return result;
    }

    StaticTextureMetadata StaticSceneReader::load_texture_metadata(
        const std::filesystem::path& path) {

        auto source = util::File::open_read(path);
        static_scene_file_io::Reader reader{
            source,
            util::File::size(path),
            path
        };
        const auto header = static_scene_file_io::read_texture_header(reader);

        StaticTextureMetadata result;
        reader.read(result.strings);
        reader.read(result.texture_payload_refs);
        reader.read(result.texture_mips);
        reader.read(result.textures);
        result.texture_payload = {
            .path = path,
            .file_offset = static_scene_file_io::texture_header_size() +
                header.metadata_size,
            .size = header.payload_size
        };
        return result;
    }

    std::filesystem::path StaticSceneReader::texture_path(
        const std::filesystem::path& scene_path) {

        auto result = scene_path;
        result.replace_extension(L".fjtex");
        return result;
    }

} // namespace fjr::scene
