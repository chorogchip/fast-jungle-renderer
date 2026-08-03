#include "FastJungle/scene/StaticSceneValidation.hpp"

#include "FastJungle/core/util/Logger.hpp"

namespace fjr::scene {

    void StaticSceneValidator::validate(const StaticScene& scene) {
        validate(scene, scene.texture_data.size());
    }

    void StaticSceneValidator::validate(
        const StaticScene& scene,
        std::uint64_t texture_payload_size) {

        for (const auto& texture : scene.textures) {
            require_range(
                texture.mip_offset,
                texture.mip_count,
                scene.texture_mips.size(),
                "texture mip");
            require_range(
                texture.data_byte_offset,
                texture.data_size,
                texture_payload_size,
                "texture data",
                StaticScene::INVALID_INDEX_64);

            for (std::uint32_t index = 0;
                 index < texture.mip_count;
                 ++index) {
                const auto& mip = scene.texture_mips[
                    texture.mip_offset + index];
                require_range(
                    mip.data_byte_offset_local,
                    mip.slice_pitch,
                    texture.data_size,
                    "texture mip data",
                    StaticScene::INVALID_INDEX_64);
            }
        }
    }

    void StaticSceneValidator::require_range(
        std::uint64_t offset,
        std::uint64_t count,
        std::uint64_t size,
        std::string_view subject,
        std::uint64_t invalid_offset) {

        if (count == 0 && offset == invalid_offset) {
            return;
        }
        if (offset <= size && count <= size - offset) {
            return;
        }
        log::Logger::g_logger
            << "Invalid StaticScene " << subject
            << " range.\n";
        log::Logger::g_logger.abort();
    }

} // namespace fjr::scene
