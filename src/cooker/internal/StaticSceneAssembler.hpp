#pragma once

#include "SceneHandles.hpp"

#include "FastJungle/cooker/StaticSceneBuilder.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fjr::cooker::internal {

    class StaticSceneAssembler final {
    public:
        StaticSceneAssembler();

        StaticSceneAssembler(const StaticSceneAssembler&) = delete;
        StaticSceneAssembler& operator=(const StaticSceneAssembler&) = delete;

        [[nodiscard]] StringOffset intern_string(std::string_view value);

        [[nodiscard]] SamplerId intern_sampler(
            const scene::StaticScene::Sampler& sampler);

        [[nodiscard]] TextureId intern_texture(
            const std::filesystem::path& path);

        [[nodiscard]] TextureBindingId append_texture_binding(
            const scene::StaticScene::TextureBinding& binding);

        [[nodiscard]] MaterialId append_material(
            const scene::StaticScene::Material& material);

        void set_camera(const scene::StaticScene::Camera& camera);

        void set_environment_light(
            const scene::StaticScene::EnvironmentLight& light);

        // Domain compilers write through this build-only storage. Ownership,
        // interning and final release remain centralized in the assembler.
        [[nodiscard]] scene::StaticScene& storage() noexcept;
        [[nodiscard]] const scene::StaticScene& storage() const noexcept;

        [[nodiscard]] StaticSceneBuild finish();

    private:
        struct SamplerKey final {
            scene::StaticScene::Sampler value;

            bool operator==(const SamplerKey& other) const noexcept;
        };

        struct SamplerKeyHash final {
            [[nodiscard]] std::size_t operator()(
                const SamplerKey& key) const noexcept;
        };

        std::unique_ptr<scene::StaticScene> scene_;
        std::vector<std::string> texture_paths_;
        std::unordered_map<std::string, std::uint32_t> string_offsets_;
        std::unordered_map<std::string, TextureId> texture_cache_;
        std::unordered_map<SamplerKey, SamplerId, SamplerKeyHash>
            sampler_cache_;
    };

} // namespace fjr::cooker::internal
