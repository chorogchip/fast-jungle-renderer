#include "FastJungle/renderer/pass/PassGPUCull.hpp"

#include <algorithm>
#include <array>
#include <filesystem>

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    namespace {

        enum class RootParameter : uint32_t {
            DISPATCH_CONSTANTS,
            CAMERA,
            SPATIAL_CLUSTERS,
            INSTANCES,
            MESHES,
            MESH_LODS,
            SUBMESHES,
            INDIRECT_DRAWS,
            INDIRECT_DRAW_COUNTS,
            VISIBLE_INSTANCES,
            BIN_COUNTS,
            BIN_OFFSETS,
            CLUSTER_BIN_BASES,
            CULL_RESULTS,
            SOFTWARE_BATCHES,
            SOFTWARE_BATCH_COUNT,
            COUNT,
        };

    } // namespace

    void PassGPUCull::init(
        ID3D12Device* device,
        dx::DescriptorHeap& heap_uav,
        uint32_t mesh_lod_count,
        uint32_t spatial_cluster_count,
        uint32_t instance_count,
        uint32_t indirect_draw_capacity_per_class) {

        indirect_draw_capacity_per_class_ =
            std::max(indirect_draw_capacity_per_class, 1u);

        const UINT64 bin_byte_size =
            static_cast<UINT64>(std::max(mesh_lod_count, 1u)) *
            sizeof(uint32_t);

        bin_counts_.init(
            device,
            bin_byte_size,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        bin_offsets_.init(
            device,
            bin_byte_size,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        const UINT64 cluster_bin_base_byte_size =
            static_cast<UINT64>(std::max(spatial_cluster_count, 1u)) *
            data::Consts::CULL_RESERVATION_STRIDE * sizeof(uint32_t);

        cluster_bin_bases_.init(
            device,
            cluster_bin_base_byte_size,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        cull_results_.init(
            device,
            instance_count * sizeof(uint16_t),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        cull_result_uav_ = heap_uav.alloc();
        cull_results_.create_typed_uav(
            device, cull_result_uav_.get_cpu(), DXGI_FORMAT_R16_UINT, 0, instance_count);

        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(D3D12_ROOT_SIGNATURE_FLAG_NONE);

        root_builder.set_constants(RootParameter::DISPATCH_CONSTANTS)
            .reg(1).count(2).vis_all().add();
        root_builder.set_root_cbv(RootParameter::CAMERA)
            .reg(0).vis_all().add();

        root_builder.set_root_srv(RootParameter::SPATIAL_CLUSTERS)
            .reg(0).vis_all().add();
        root_builder.set_root_srv(RootParameter::INSTANCES)
            .reg(1).vis_all().add();
        root_builder.set_root_srv(RootParameter::MESHES)
            .reg(2).vis_all().add();
        root_builder.set_root_srv(RootParameter::MESH_LODS)
            .reg(3).vis_all().add();
        root_builder.set_root_srv(RootParameter::SUBMESHES)
            .reg(4).vis_all().add();

        root_builder.set_root_uav(RootParameter::INDIRECT_DRAWS)
            .reg(0).vis_all().add();
        root_builder.set_root_uav(RootParameter::INDIRECT_DRAW_COUNTS)
            .reg(1).vis_all().add();
        root_builder.set_root_uav(RootParameter::VISIBLE_INSTANCES)
            .reg(2).vis_all().add();
        root_builder.set_root_uav(RootParameter::BIN_COUNTS)
            .reg(3).vis_all().add();
        root_builder.set_root_uav(RootParameter::BIN_OFFSETS)
            .reg(4).vis_all().add();
        root_builder.set_root_uav(RootParameter::CLUSTER_BIN_BASES)
            .reg(5).vis_all().add();
        root_builder.set_resource_table(RootParameter::CULL_RESULTS)
            .uav().reg(6).count(1).add_range()
            .vis_all().add();
        root_builder.set_root_uav(RootParameter::SOFTWARE_BATCHES)
            .reg(7).vis_all().add();
        root_builder.set_root_uav(RootParameter::SOFTWARE_BATCH_COUNT)
            .reg(8).vis_all().add();

        root_signature_ = root_builder.build(device);

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR };

        dx::Shader shader;
        auto description = dx::PSOUtils::default_compute_desc();
        description.pRootSignature = root_signature_.Get();

        shader.load(shader_directory / "culling" / "Clear.cs.dxil");
        description.CS = shader.get_bytecode();
        clear_pipeline_ = dx::PSOUtils::create_compute(device, description);

        shader.load(shader_directory / "culling" / "CullCount.cs.dxil");
        description.CS = shader.get_bytecode();
        count_pipeline_ = dx::PSOUtils::create_compute(device, description);

        shader.load(shader_directory / "culling" / "BinScan.cs.dxil");
        description.CS = shader.get_bytecode();
        scan_pipeline_ = dx::PSOUtils::create_compute(device, description);

        shader.load(shader_directory / "culling" / "CullScatter.cs.dxil");
        description.CS = shader.get_bytecode();
        scatter_pipeline_ = dx::PSOUtils::create_compute(device, description);

        shader.load(shader_directory / "culling" / "BuildIndirect.cs.dxil");
        description.CS = shader.get_bytecode();
        build_pipeline_ = dx::PSOUtils::create_compute(device, description);
    }

    void PassGPUCull::record(
        dx::CommandContext& context,
        const data::DataPersistent& persistent,
        data::DataPerFrame& frame) {

        auto* command_list = context.get();

        context.transition(
            frame.indirect_gpu_draw,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.transition(
            frame.indirect_gpu_draw_counts,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.transition(
            frame.software_batches,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.transition(
            frame.software_batch_count,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        context.transition(
            frame.visible_instance,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        command_list->SetComputeRootSignature(root_signature_.Get());

        const std::array<uint32_t, 2> dispatch_constants{
            indirect_draw_capacity_per_class_,
            data::Consts::RASTER_CLASS_CNT,
        };
        command_list->SetComputeRoot32BitConstants(
            static_cast<UINT>(RootParameter::DISPATCH_CONSTANTS),
            static_cast<UINT>(dispatch_constants.size()),
            dispatch_constants.data(),
            0);
        command_list->SetComputeRootConstantBufferView(
            static_cast<UINT>(RootParameter::CAMERA),
            frame.camera.get_address());

        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::SPATIAL_CLUSTERS),
            persistent.spatial_cluster->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::INSTANCES),
            persistent.instance_transform->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::MESHES),
            persistent.mesh->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::MESH_LODS),
            persistent.mesh_lod->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::SUBMESHES),
            persistent.submesh->GetGPUVirtualAddress());

        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::INDIRECT_DRAWS),
            frame.indirect_gpu_draw->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::INDIRECT_DRAW_COUNTS),
            frame.indirect_gpu_draw_counts->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::VISIBLE_INSTANCES),
            frame.visible_instance->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::BIN_COUNTS),
            bin_counts_->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::BIN_OFFSETS),
            bin_offsets_->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::CLUSTER_BIN_BASES),
            cluster_bin_bases_->GetGPUVirtualAddress());
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::CULL_RESULTS),
            cull_result_uav_.get_gpu());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::SOFTWARE_BATCHES),
            frame.software_batches->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::SOFTWARE_BATCH_COUNT),
            frame.software_batch_count->GetGPUVirtualAddress());

        command_list->SetPipelineState(clear_pipeline_.Get());
        context.Dispatch<256>((std::max)(
            persistent.mesh_lod_count,
            data::Consts::RASTER_CLASS_CNT));
        context.uav_barrier(bin_counts_);

        command_list->SetPipelineState(count_pipeline_.Get());
        context.Dispatch((std::max)(persistent.spatial_cluster_count, 1u));
        context.uav_barrier(bin_counts_);

        command_list->SetPipelineState(scan_pipeline_.Get());
        context.Dispatch(1, 1, 1);
        context.uav_barrier(bin_offsets_);
        context.uav_barrier(cluster_bin_bases_);
        context.uav_barrier(cull_results_);

        command_list->SetPipelineState(scatter_pipeline_.Get());
        context.Dispatch((std::max)(persistent.spatial_cluster_count, 1u));
        context.uav_barrier(frame.indirect_gpu_draw_counts);
        context.uav_barrier(frame.software_batch_count);

        command_list->SetPipelineState(build_pipeline_.Get());
        context.Dispatch<256>(persistent.mesh_lod_count);

        context.transition(
            frame.indirect_gpu_draw,
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        context.transition(
            frame.indirect_gpu_draw_counts,
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        context.transition(
            frame.software_batches,
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        context.transition(
            frame.software_batch_count,
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        context.transition(
            frame.visible_instance,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        context.uav_barrier(bin_counts_);
        context.uav_barrier(bin_offsets_);
        context.uav_barrier(cluster_bin_bases_);
        context.uav_barrier(cull_results_);
    }

} // namespace fjr::render
