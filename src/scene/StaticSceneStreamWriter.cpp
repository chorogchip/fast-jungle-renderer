#include "FastJungle/scene/StaticSceneWriter.hpp"

#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/TemporaryFile.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

#include "FastJungle/scene/StaticSceneFileIO.hpp"

#include <istream>
#include <limits>

namespace fjr::scene {

    namespace {

        void add_size(
            std::uint64_t& total,
            std::uint64_t size) {

            if (size > std::numeric_limits<std::uint64_t>::max() - total) {
                log::Logger::g_logger
                    << "StaticScene output size overflow.\n";
                log::Logger::g_logger.abort();
            }
            total += size;
        }

        void add_vector_size(
            std::uint64_t& total,
            std::uint64_t count,
            std::uint64_t element_size) {

            if (element_size != 0 &&
                count > std::numeric_limits<std::uint64_t>::max() /
                    element_size) {
                log::Logger::g_logger
                    << "StaticScene vector size overflow.\n";
                log::Logger::g_logger.abort();
            }
            add_size(total, sizeof(std::size_t));
            add_size(total, count * element_size);
        }

        [[nodiscard]]
        std::uint64_t calculate_scene_file_size(
            const StaticScene& scene) {

            std::uint64_t total = static_scene_file_io::header_size();

#define X(type, name) \
            add_vector_size( \
                total, scene.name.size(), sizeof(StaticScene::type));
            SceneData_MACRO
#undef X

            add_size(total, sizeof(StaticScene::Camera));
            add_size(total, sizeof(StaticScene::EnvironmentLight));
            add_size(total, sizeof(StaticScene::SceneInfo));
            add_size(total, sizeof(StaticScene::Components));
            return total;
        }

        void save_texture(
            const std::filesystem::path& path,
            const std::vector<std::byte>* texture_data,
            std::istream* texture_payload,
            const std::filesystem::path& texture_payload_path,
            std::uint64_t texture_payload_size) {

            auto temporary_path = path;
            temporary_path += L".tmp";
            util::TemporaryFile temporary{temporary_path};
            auto output = util::File::open_write(temporary.path());
            static_scene_file_io::Writer writer{
                output,
                temporary.path()
            };

            static_scene_file_io::write_texture_header(
                writer,
                texture_payload_size);
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
                texture_payload_size;
            if (writer.offset() != expected_size) {
                log::Logger::g_logger
                    << "StaticTexture output size changed.\n";
                log::Logger::g_logger.abort();
            }
            util::File::finish(output, temporary.path());
            temporary.replace(path);
        }

        void save_scene(
            const std::filesystem::path& path,
            const StaticScene& scene,
            std::uint64_t texture_payload_size) {

            auto temporary_path = path;
            temporary_path += L".tmp";
            util::TemporaryFile temporary{temporary_path};
            auto output = util::File::open_write(temporary.path());
            static_scene_file_io::Writer writer{
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
            &scene.texture_data,
            nullptr,
            {},
            scene.texture_data.size());
        save_scene(path, scene, scene.texture_data.size());
    }

    void StaticSceneWriter::save(
        const std::filesystem::path& path,
        const StaticScene& scene,
        const std::filesystem::path& texture_payload_path,
        std::uint64_t texture_payload_size) {

        if (!scene.texture_data.empty()) {
            log::Logger::g_logger
                << "External texture payload requires empty scene data.\n";
            log::Logger::g_logger.abort();
        }
        StaticSceneValidator::validate(scene, texture_payload_size);
        util::File::require_size(
            texture_payload_path,
            texture_payload_size);
        auto texture_payload = util::File::open_read(texture_payload_path);
        save_texture(
            texture_path(path),
            nullptr,
            &texture_payload,
            texture_payload_path,
            texture_payload_size);
        save_scene(path, scene, texture_payload_size);
    }

    std::filesystem::path StaticSceneWriter::texture_path(
        const std::filesystem::path& scene_path) {

        auto result = scene_path;
        result.replace_extension(L".fjtex");
        return result;
    }

} // namespace fjr::scene
