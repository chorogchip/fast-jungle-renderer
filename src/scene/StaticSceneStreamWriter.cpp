#include "FastJungle/scene/StaticSceneWriter.hpp"

#include "FastJungle/core/util/BinaryIO.hpp"
#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/TemporaryFile.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

#include "FastJungle/scene/StaticSceneFileIO.hpp"

#include <istream>
namespace fjr::scene {

    namespace {

        [[nodiscard]]
        uint64_t calculate_scene_file_size(
            const StaticScene& scene) {

            util::BinarySize size;
            size.add(static_scene_file_io::header_size());

#define X(type, name) \
            size.add_vector( \
                scene.name.size(), sizeof(StaticScene::type));
            SceneData_MACRO
#undef X

            size.add(sizeof(StaticScene::Camera));
            size.add(sizeof(StaticScene::EnvironmentLight));
            size.add(sizeof(StaticScene::SceneInfo));
            size.add(sizeof(StaticScene::Components));
            return size.value();
        }

        [[nodiscard]]
        uint64_t calculate_texture_metadata_size(
            const StaticScene& scene) {

            util::BinarySize size;
            size.add_vector(
                scene.strings.size(),
                sizeof(StaticScene::Char));
            size.add_vector(
                scene.texture_payload_refs.size(),
                sizeof(StaticScene::TexturePayloadRef));
            size.add_vector(
                scene.texture_mips.size(),
                sizeof(StaticScene::TextureMip));
            size.add_vector(
                scene.textures.size(),
                sizeof(StaticScene::Texture));
            return size.value();
        }

        void save_texture(
            const std::filesystem::path& path,
            const StaticScene& scene,
            const std::vector<std::byte>* texture_data,
            std::istream* texture_payload,
            const std::filesystem::path& texture_payload_path,
            uint64_t texture_payload_size) {

            auto temporary_path = path;
            temporary_path += L".tmp";
            util::TemporaryFile temporary{temporary_path};
            auto output = util::File::open_write(temporary.path());
            util::BinaryWriter writer{
                output,
                temporary.path()
            };

            static_scene_file_io::write_texture_header(
                writer,
                calculate_texture_metadata_size(scene),
                texture_payload_size);
            writer.write(scene.strings);
            writer.write(scene.texture_payload_refs);
            writer.write(scene.texture_mips);
            writer.write(scene.textures);
            if (texture_data != nullptr) {
                writer.write_raw(
                    texture_data->data(),
                    texture_data->size());
            }
            else {
                writer.copy(
                    *texture_payload,
                    texture_payload_size,
                    texture_payload_path);
            }

            const auto expected_size =
                static_scene_file_io::texture_header_size() +
                calculate_texture_metadata_size(scene) +
                texture_payload_size;
            if (writer.offset() != expected_size) {
                log::Logger::g_logger
                    << "StaticTexture output size changed.\n";
                log::Logger::g_logger.abort();
            }
            util::File::finish(output, temporary.path());
            temporary.replace(path);
        }

        void save_scene_file(
            const std::filesystem::path& path,
            const StaticScene& scene,
            uint64_t texture_payload_size) {

            auto temporary_path = path;
            temporary_path += L".tmp";
            util::TemporaryFile temporary{temporary_path};
            auto output = util::File::open_write(temporary.path());
            util::BinaryWriter writer{
                output,
                temporary.path()
            };

            const auto size = calculate_scene_file_size(scene);
            static_scene_file_io::write_header(
                writer,
                size - static_scene_file_io::header_size(),
                texture_payload_size);

#define X(type, name) writer.write(scene.name);
            SceneData_MACRO
#undef X

            writer.write(scene.components);
            writer.write(scene.camera);
            writer.write(scene.environment_light);
            writer.write(scene.info);
            if (writer.offset() != size) {
                log::Logger::g_logger
                    << "StaticScene output size changed.\n";
                log::Logger::g_logger.abort();
            }
            util::File::finish(output, temporary.path());
            temporary.replace(path);
        }

    } // namespace

    void StaticSceneWriter::save(
        const std::filesystem::path& path,
        const StaticScene& scene) {

        StaticSceneValidator::validate(scene);
        save_texture(
            texture_path(path),
            scene,
            &scene.texture_data,
            nullptr,
            {},
            scene.texture_data.size());
        save_scene_file(path, scene, scene.texture_data.size());
    }

    void StaticSceneWriter::save(
        const std::filesystem::path& path,
        const StaticScene& scene,
        StaticTexturePayload texture_payload) {

        if (!scene.texture_data.empty()) {
            log::Logger::g_logger
                << "External texture payload requires empty scene data.\n";
            log::Logger::g_logger.abort();
        }
        StaticSceneValidator::validate(scene, texture_payload.size);
        util::File::require_size(
            texture_payload.file.path(),
            texture_payload.size);
        {
            auto texture_source = util::File::open_read(
                texture_payload.file.path());
            save_texture(
                texture_path(path),
                scene,
                nullptr,
                &texture_source,
                texture_payload.file.path(),
                texture_payload.size);
        }
        save_scene_file(path, scene, texture_payload.size);
        texture_payload.file.remove();
    }

    std::filesystem::path StaticSceneWriter::texture_path(
        const std::filesystem::path& scene_path) {

        auto result = scene_path;
        result.replace_extension(L".fjtex");
        return result;
    }

} // namespace fjr::scene
