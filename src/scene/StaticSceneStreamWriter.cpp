#include "FastJungle/scene/StaticSceneWriter.hpp"

#include "FastJungle/core/util/BinaryStream.hpp"
#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/TemporaryFile.hpp"

#include "FastJungle/scene/StaticSceneFileFormat.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

#include <istream>
#include <limits>

namespace fjr::scene {

    namespace {

        class SceneOutput final {
        public:
            static void save(
                const std::filesystem::path& path,
                const StaticScene& scene,
                std::istream* texture_payload,
                const std::filesystem::path& texture_payload_path,
                std::uint64_t texture_payload_size) {

                auto temporary_path = path;
                temporary_path += L".tmp";
                util::TemporaryFile temporary{temporary_path};
                auto output = util::File::open_write(temporary.path());
                util::BinaryWriter writer{
                    output,
                    temporary.path()
                };

                write(
                    writer,
                    scene,
                    texture_payload,
                    texture_payload_path,
                    texture_payload_size);
                util::File::finish(output, temporary.path());
                temporary.replace(path);
            }

        private:
            static void write(
                util::BinaryWriter& writer,
                const StaticScene& scene,
                std::istream* texture_payload,
                const std::filesystem::path& texture_payload_path,
                std::uint64_t texture_payload_size) {

                const std::uint64_t size =
                    StaticSceneWriter::calculate_size(
                        scene,
                        texture_payload_size);
                StaticSceneFileFormat::write_header(
                    writer,
                    size - StaticSceneFileFormat::header_size());

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
        };

    } // namespace

    void StaticSceneWriter::save(
        const std::filesystem::path& path,
        const StaticScene& scene) {

        StaticSceneValidator::validate(scene);
        SceneOutput::save(
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
        auto texture_payload = util::File::open_read(
            texture_payload_path);
        SceneOutput::save(
            path,
            scene,
            &texture_payload,
            texture_payload_path,
            texture_payload_size);
    }

    std::uint64_t StaticSceneWriter::calculate_size(
        const StaticScene& scene,
        std::uint64_t texture_payload_size) {

        std::uint64_t total = StaticSceneFileFormat::header_size();

#define X(type, name) \
        add_vector(total, scene.name.size(), sizeof(StaticScene::type));
        SceneDataBeforeTexture_MACRO
#undef X

        add(total, sizeof(std::size_t));
        add(total, texture_payload_size);

#define X(type, name) \
        add_vector(total, scene.name.size(), sizeof(StaticScene::type));
        SceneDataAfterTexture_MACRO
#undef X

        add(total, sizeof(StaticScene::Camera));
        add(total, sizeof(StaticScene::EnvironmentLight));
        add(total, sizeof(StaticScene::SceneInfo));
        return total;
    }

    void StaticSceneWriter::add(
        std::uint64_t& total,
        std::uint64_t size) {

        if (size > std::numeric_limits<std::uint64_t>::max() - total) {
            log::Logger::g_logger
                << "StaticScene output size overflow.\n";
            log::Logger::g_logger.abort();
        }
        total += size;
    }

    void StaticSceneWriter::add_vector(
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
        add(total, sizeof(std::size_t));
        add(total, count * element_size);
    }

} // namespace fjr::scene
