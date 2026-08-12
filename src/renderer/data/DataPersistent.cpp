#include "FastJungle/renderer/data/DataPersistent.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "FastJungle/renderer/data/geometry/BuilderGeometry.hpp"
#include "FastJungle/renderer/data/material/BuilderMaterial.hpp"
#include "FastJungle/renderer/data/BuilderSpatial.hpp"

namespace fjr::render::data {

    DataPersistent DataPersistent::build(
        const scene::StaticScene& scene,
        ID3D12Device* device,
        dx::ResourceUploader& uploader,
        dx::DescriptorHeap& heap_srv_cbv_uav,
        dx::DescriptorHeap& heap_sampler) {

        data::DataPersistent result{};

        const auto geometry = BuilderGeometry::build(
            result, uploader, device, scene);

        BuilderMaterial::build(
            result,
            uploader, device,
            heap_srv_cbv_uav, heap_sampler, scene,
            geometry.meshes);

        const auto spatial = BuilderSpatial::build(
            result,
            uploader, device, scene,
            geometry.meshes);

        result.instance_count = spatial.instance_count;
        result.spatial_cluster_count = spatial.spatial_cluster_count;
        result.mesh_lod_count = static_cast<uint32_t>(scene.mesh_lods.size());
        result.submesh_count = static_cast<uint32_t>(scene.submeshes.size());

        return result;
    }

} // namespace fjr::render
