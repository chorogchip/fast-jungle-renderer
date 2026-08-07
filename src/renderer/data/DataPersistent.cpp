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


        void init_dynamic_buffer(
            dx::Buffer& output,
            ID3D12Device* device,
            UINT64 byte_size) {

            if (byte_size == 0) {
                return;
            }

            output.init(
                device,
                byte_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COMMON);
        }

        void allocate_dynamic_resources(
            data::DataPersistent::Dynamic& output,
            ID3D12Device* device,
            std::uint32_t instance_count,
            std::uint32_t mesh_lod_count) {

            using Dynamic = data::DataPersistent::Dynamic;


            // Fixed slab per raster class.
            constexpr std::uint32_t MAX_INDIRECT_DRAW_COUNT_PER_CLASS =
                256u * data::Consts::LOD_CNT;


            const UINT64 indirect_capacity =
                static_cast<UINT64>(
                    MAX_INDIRECT_DRAW_COUNT_PER_CLASS) *
                data::Consts::RASTER_CLASS_CNT;

            init_dynamic_buffer(
                output.indirect_gpu_draw,
                device,
                indirect_capacity *
                sizeof(Dynamic::IndirectGPUDraw));

            init_dynamic_buffer(
                output.indirect_gpu_draw_counts,
                device,
                static_cast<UINT64>(
                    data::Consts::RASTER_CLASS_CNT) *
                sizeof(std::uint32_t));

            init_dynamic_buffer(
                output.visible_instance,
                device,
                static_cast<UINT64>(instance_count) *
                sizeof(std::uint32_t));

            const UINT64 bin_byte_size =
                static_cast<UINT64>(mesh_lod_count) *
                sizeof(std::uint32_t);

            init_dynamic_buffer(
                output.bin_counts,
                device,
                bin_byte_size);

            init_dynamic_buffer(
                output.bin_offsets,
                device,
                bin_byte_size);

            init_dynamic_buffer(
                output.bin_cursors,
                device,
                bin_byte_size);
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
            result.fixed, uploader, device, scene);

        BuilderMaterial::build(
            result.fixed,
            uploader, device,
            heap_srv_cbv_uav, heap_sampler, scene);

        const auto spatial = BuilderSpatial::build(
            result.fixed,
            uploader, device, scene,
            geometry.meshes);

        allocate_dynamic_resources(
            result.dynamic,
            device,
            spatial.instance_count,
            static_cast<UINT>(scene.mesh_lods.size()));

        return result;
    }

} // namespace fjr::render