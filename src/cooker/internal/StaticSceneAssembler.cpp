#include "StaticSceneAssembler.hpp"

#include "PathKey.hpp"
#include "CookError.hpp"

#include "FastJungle/core/math/CheckedCast.hpp"

#include <cstdint>
#include <utility>

namespace fjr::cooker::internal {

    using StaticScene = scene::StaticScene;

    bool StaticSceneAssembler::SamplerKey::operator==(
        const SamplerKey& other) const noexcept {

        return value.filter == other.value.filter &&
            value.address_u == other.value.address_u &&
            value.address_v == other.value.address_v &&
            value.address_w == other.value.address_w &&
            value.max_anisotropy == other.value.max_anisotropy;
    }

    std::size_t StaticSceneAssembler::SamplerKeyHash::operator()(
        const SamplerKey& key) const noexcept {

        std::size_t hash = static_cast<std::size_t>(key.value.filter);
        hash = hash * 131u + static_cast<std::size_t>(key.value.address_u);
        hash = hash * 131u + static_cast<std::size_t>(key.value.address_v);
        hash = hash * 131u + static_cast<std::size_t>(key.value.address_w);
        hash = hash * 131u + key.value.max_anisotropy;
        return hash;
    }

    StaticSceneAssembler::StaticSceneAssembler()
        : scene_(std::make_unique<StaticScene>()) {

        scene_->strings.push_back('\0');
        string_offsets_.emplace(std::string{}, 0u);

        scene_->point_instances.reserve(8'674'676);
        scene_->point_category_spans.reserve(58);
        scene_->point_mesh_batches.reserve(53);
        scene_->static_mesh_instances.reserve(84);
        scene_->meshes.reserve(160);
        scene_->submeshes.reserve(256);
        scene_->materials.reserve(192);
        scene_->textures.reserve(600);
        scene_->texture_bindings.reserve(800);
    }

    StringOffset StaticSceneAssembler::intern_string(
        std::string_view value) {

        const auto existing = string_offsets_.find(std::string{value});
        if (existing != string_offsets_.end()) {
            return StringOffset{existing->second};
        }

        const auto offset = math::checked_cast<std::uint32_t>(
            scene_->strings.size(),
            "String table offset");
        scene_->strings.insert(
            scene_->strings.end(),
            value.begin(),
            value.end());
        scene_->strings.push_back('\0');
        string_offsets_.emplace(std::string{value}, offset);
        return StringOffset{offset};
    }

    SamplerId StaticSceneAssembler::intern_sampler(
        const StaticScene::Sampler& sampler) {

        const SamplerKey key{sampler};
        const auto cached = sampler_cache_.find(key);
        if (cached != sampler_cache_.end()) {
            return cached->second;
        }

        const SamplerId id{math::checked_cast<std::uint32_t>(
            scene_->samplers.size(),
            "Sampler index")};
        scene_->samplers.push_back(sampler);
        sampler_cache_.emplace(key, id);
        return id;
    }

    TextureId StaticSceneAssembler::intern_texture(
        const std::filesystem::path& path) {

        const auto key = normalized_path_key(path);
        const auto cached = texture_cache_.find(key);
        if (cached != texture_cache_.end()) {
            return cached->second;
        }

        StaticScene::Texture texture;
        texture.name = intern_string(
            path.filename().generic_string()).value();
        const TextureId id{math::checked_cast<std::uint32_t>(
            scene_->textures.size(),
            "Texture index")};
        scene_->textures.push_back(texture);
        texture_paths_.push_back(path.generic_string());
        texture_cache_.emplace(key, id);
        return id;
    }

    TextureBindingId StaticSceneAssembler::append_texture_binding(
        const StaticScene::TextureBinding& binding) {

        const TextureBindingId id{math::checked_cast<std::uint32_t>(
            scene_->texture_bindings.size(),
            "Texture binding index")};
        scene_->texture_bindings.push_back(binding);
        return id;
    }

    MaterialId StaticSceneAssembler::append_material(
        const StaticScene::Material& material) {

        const MaterialId id{math::checked_cast<std::uint32_t>(
            scene_->materials.size(),
            "Material index")};
        scene_->materials.push_back(material);
        return id;
    }

    void StaticSceneAssembler::set_camera(
        const StaticScene::Camera& camera) {

        if (scene_->camera.name != StaticScene::INVALID_INDEX) {
            fail("Jungle camera component produced multiple cameras.");
        }
        scene_->camera = camera;
    }

    void StaticSceneAssembler::set_environment_light(
        const StaticScene::EnvironmentLight& light) {

        if (scene_->environment_light.name != StaticScene::INVALID_INDEX) {
            fail("Jungle root produced multiple environment lights.");
        }
        scene_->environment_light = light;
    }

    StaticScene& StaticSceneAssembler::storage() noexcept {
        return *scene_;
    }

    const StaticScene& StaticSceneAssembler::storage() const noexcept {
        return *scene_;
    }

    StaticSceneBuild StaticSceneAssembler::finish() {
        scene_->info.vertex_count_after_indexing = scene_->vertices.size();
        return {
            .scene = std::move(scene_),
            .texture_paths = std::move(texture_paths_),
        };
    }

} // namespace fjr::cooker::internal
