#include "FastJungle/renderer/data/material/BuilderMaterial.hpp"

#include <span>

#include "FastJungle/renderer/data/material/BuilderMatSampler.hpp"
#include "FastJungle/renderer/data/material/BuilderMatTable.hpp"
#include "FastJungle/renderer/data/material/BuilderMatTexture.hpp"

namespace fjr::render::data {

    void BuilderMaterial::build(
        DataPersistent& output,
        dx::ResourceUploader& uploader,
        ID3D12Device* device,
        dx::DescriptorHeap& heap_srv_cbv_uav,
        dx::DescriptorHeap& heap_sampler,
        const scene::StaticScene& scene,
        std::span<const DataPersistent::Mesh> meshes) {

        mat::BuilderMatTexture::build(
            output,
            uploader,
            device,
            heap_srv_cbv_uav,
            scene);

        mat::BuilderMatSampler::build(
            output,
            device,
            heap_sampler,
            scene);

        const auto materials = mat::BuilderMatTable::build(scene, meshes);

        output.material.init(
            device,
            static_cast<UINT64>(
                materials.size() * sizeof(DataPersistent::Material)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.material,
            materials,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

} // namespace fjr::render::data
