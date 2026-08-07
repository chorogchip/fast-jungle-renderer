#include "FastJungle/renderer/data/DataPersistent.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "FastJungle/renderer/data/BuilderGeometry.hpp"
#include "FastJungle/renderer/data/BuilderMaterial.hpp"
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
            heap_srv_cbv_uav, heap_sampler, scene);

        const auto spatial = BuilderSpatial::build(
            result,
            uploader, device, scene,
            geometry.meshes);

        result.instnace_cnt = spatial.instance_count;
        result.bin_cnt = spatial.spatial_cluster_count;

        return result;
    }

} // namespace fjr::render