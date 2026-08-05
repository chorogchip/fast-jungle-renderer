#pragma once

struct ID3D12Device;

namespace fjr::dx {
    class ResourceUploader;
}

namespace fjr::scene {
    class StaticScene;
} // namespace fjr::scene

namespace fjr::render {
    class SceneResources;

    void create_scene_texture_resources(
        SceneResources& resources,
        dx::ResourceUploader& uploader,
        ID3D12Device* device,
        const scene::StaticScene& scene);

} // namespace fjr::render
