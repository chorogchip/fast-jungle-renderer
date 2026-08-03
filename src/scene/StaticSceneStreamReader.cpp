#include "FastJungle/scene/StaticSceneReader.hpp"

#include "FastJungle/core/util/BinaryStream.hpp"
#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"

#include "FastJungle/scene/StaticSceneFileFormat.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

namespace fjr::scene {

    std::unique_ptr<StaticScene> StaticSceneReader::load(
        const std::filesystem::path& path) {

        auto source = util::File::open_read(path);
        util::BinaryReader reader{source, util::File::size(path), path};

        StaticSceneFileFormat::read_header(reader);

        std::unique_ptr<StaticScene> scene;
        try {
            scene = std::make_unique<StaticScene>();
        }
        catch (...) {
            log::Logger::g_logger
                << "Failed to allocate StaticScene: " << path << '\n';
            log::Logger::g_logger.abort();
        }

#define X(type, name) reader.read(scene->name);
        SceneData_MACRO
#undef X

        reader.read(scene->camera);
        reader.read(scene->environment_light);
        reader.read(scene->info);
        reader.require_end();
        StaticSceneValidator::validate(*scene);
        return scene;
    }

    StaticSceneMetadata StaticSceneReader::load_metadata(
        const std::filesystem::path& path) {

        auto source = util::File::open_read(path);
        util::BinaryReader reader{source, util::File::size(path), path};

        StaticSceneFileFormat::read_header(reader);

        StaticSceneMetadata result;
        try {
            result.scene = std::make_unique<StaticScene>();
        }
        catch (...) {
            log::Logger::g_logger
                << "Failed to allocate StaticScene metadata: "
                << path << '\n';
            log::Logger::g_logger.abort();
        }

#define X(type, name) reader.read(result.scene->name);
        SceneDataBeforeTexture_MACRO
#undef X

        std::size_t texture_payload_size = 0;
        reader.read(texture_payload_size);
        result.texture_payload = {
            .file_offset = reader.offset(),
            .size = texture_payload_size
        };
        reader.skip(texture_payload_size);

#define X(type, name) reader.read(result.scene->name);
        SceneDataAfterTexture_MACRO
#undef X

        reader.read(result.scene->camera);
        reader.read(result.scene->environment_light);
        reader.read(result.scene->info);
        reader.require_end();
        StaticSceneValidator::validate(
            *result.scene,
            result.texture_payload.size);
        return result;
    }

} // namespace fjr::scene
