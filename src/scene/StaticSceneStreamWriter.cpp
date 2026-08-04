#include "FastJungle/scene/StaticSceneWriter.hpp"

#include "FastJungle/core/util/BinaryStream.hpp"
#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/TemporaryFile.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

#include "StaticSceneFileHeader.hpp"

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
        std::uint64_t calculate_file_size(
            const StaticScene& scene,
            std::uint64_t texture_payload_size) {

            std::uint64_t total = static_scene_file_header::size();

#define X(type, name) \
            add_vector_size( \
                total, scene.name.size(), sizeof(StaticScene::type));
            SceneDataBeforeTexture_MACRO
#undef X

            add_size(total, sizeof(std::size_t));
            add_size(total, texture_payload_size);

#define X(type, name) \
            add_vector_size( \
                total, scene.name.size(), sizeof(StaticScene::type));
            SceneDataAfterTexture_MACRO
#undef X

            add_size(total, sizeof(StaticScene::Camera));
            add_size(total, sizeof(StaticScene::EnvironmentLight));
            add_size(total, sizeof(StaticScene::SceneInfo));
            return total;
        }

        void write_scene(
            util::BinaryWriter& writer,
            const StaticScene& scene,
            std::istream* texture_payload,
            const std::filesystem::path& texture_payload_path,
            std::uint64_t texture_payload_size) {

            const std::uint64_t size = calculate_file_size(
                scene,
                texture_payload_size);
            static_scene_file_header::write(
                writer,
                size - static_scene_file_header::size());

#define X(type, name) writer.write(scene.name);
            SceneDataBeforeTexture_MACRO
#undef X

            if (texture_payload == nullptr) {
                writer.write(scene.texture_data);
            }
            else {
                if (texture_payload_size >
                    std::numeric_limits<std::size_t>::max()) {
                    log::Logger::g_logger
                        << "Texture payload is too large: "
                        << texture_payload_path << '\n';
                    log::Logger::g_logger.abort();
                }
                const auto stored_size =
                    static_cast<std::size_t>(texture_payload_size);
                writer.write(stored_size);
                writer.copy(
                    *texture_payload,
                    texture_payload_size,
                    texture_payload_path);
            }

#define X(type, name) writer.write(scene.name);
            SceneDataAfterTexture_MACRO
#undef X

            writer.write(scene.camera);
            writer.write(scene.environment_light);
            writer.write(scene.info);
            if (writer.offset() != size) {
                log::Logger::g_logger
                    << "StaticScene output size changed.\n";
                log::Logger::g_logger.abort();
            }
        }

        void save_scene(
            const std::filesystem::path& path,
            const StaticScene& scene,
            std::istream* texture_payload,
            const std::filesystem::path& texture_payload_path,
            std::uint64_t texture_payload_size) {

            auto temporary_path = path;
            temporary_path += L".tmp";
            util::TemporaryFile temporary{temporary_path};
            auto output = util::File::open_write(temporary.path());
            util::BinaryWriter writer{output, temporary.path()};

            write_scene(
                writer,
                scene,
                texture_payload,
                texture_payload_path,
                texture_payload_size);
            util::File::finish(output, temporary.path());
            temporary.replace(path);
        }

    } // namespace

    void StaticSceneWriter::save(
        const std::filesystem::path& path,
        const StaticScene& scene) {

        StaticSceneValidator::validate(scene);
        save_scene(
            path,
            scene,
            nullptr,
            {},
            scene.texture_data.size());
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
        save_scene(
            path,
            scene,
            &texture_payload,
            texture_payload_path,
            texture_payload_size);
    }

} // namespace fjr::scene
