#include "FastJungle/scene/StaticSceneReader.hpp"

#include "FastJungle/core/util/BinaryStream.hpp"
#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"

#include "FastJungle/scene/StaticSceneValidation.hpp"

#include "StaticSceneFileHeader.hpp"

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

        void read_before_texture(
            util::BinaryReader& reader,
            StaticScene& scene) {

#define X(type, name) reader.read(scene.name);
            SceneDataBeforeTexture_MACRO
#undef X
        }

        void read_after_texture(
            util::BinaryReader& reader,
            StaticScene& scene) {

#define X(type, name) reader.read(scene.name);
            SceneDataAfterTexture_MACRO
#undef X

            reader.read(scene.camera);
            reader.read(scene.environment_light);
            reader.read(scene.info);
        }

    } // namespace

    std::unique_ptr<StaticScene> StaticSceneReader::load(
        const std::filesystem::path& path) {

        auto source = util::File::open_read(path);
        util::BinaryReader reader{source, util::File::size(path), path};

        static_scene_file_header::read(reader);

        auto scene = allocate_scene(path);
        read_before_texture(reader, *scene);
        reader.read(scene->texture_data);
        read_after_texture(reader, *scene);
        reader.require_end();
        StaticSceneValidator::validate(*scene);
        return scene;
    }

    StaticSceneMetadata StaticSceneReader::load_metadata(
        const std::filesystem::path& path) {

        auto source = util::File::open_read(path);
        util::BinaryReader reader{source, util::File::size(path), path};

        static_scene_file_header::read(reader);

        StaticSceneMetadata result;
        result.scene = allocate_scene(path);
        read_before_texture(reader, *result.scene);

        std::size_t texture_payload_size = 0;
        reader.read(texture_payload_size);
        result.texture_payload = {
            .file_offset = reader.offset(),
            .size = texture_payload_size
        };
        reader.skip(texture_payload_size);

        read_after_texture(reader, *result.scene);
        reader.require_end();
        StaticSceneValidator::validate(
            *result.scene,
            result.texture_payload.size);
        return result;
    }

} // namespace fjr::scene
