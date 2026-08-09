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

    namespace {

        [[nodiscard]]
        std::uint32_t checked_u32(
            std::size_t value,
            const char* subject) {

            if (value > std::numeric_limits<std::uint32_t>::max()) {
                log::Logger::g_logger
                    << subject << " exceeds uint32_t."
                    << log::abrt();
            }
            return static_cast<std::uint32_t>(value);
        }

    } // namespace

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
        result.mesh_lod_count = checked_u32(
            scene.mesh_lods.size(),
            "Scene mesh LOD count");
        result.submesh_count = checked_u32(
            scene.submeshes.size(),
            "Scene submesh count");

        return result;
    }

} // namespace fjr::render
