#pragma once

#include "SceneHandles.hpp"

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

    class StaticSceneDataBuilder final {
    public:
        StaticSceneDataBuilder();

        StaticSceneDataBuilder(const StaticSceneDataBuilder&) = delete;
        StaticSceneDataBuilder& operator=(const StaticSceneDataBuilder&) = delete;

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

        [[nodiscard]] scene::StaticScene& storage() noexcept;
        [[nodiscard]] const scene::StaticScene& storage() const noexcept;

        [[nodiscard]] std::unique_ptr<scene::StaticScene> finish();

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
        std::unordered_map<std::string, uint32_t> string_offsets_;
        std::unordered_map<std::string, TextureId> texture_cache_;
        std::unordered_map<SamplerKey, SamplerId, SamplerKeyHash>
            sampler_cache_;
    };

} // namespace fjr::cooker::internal
