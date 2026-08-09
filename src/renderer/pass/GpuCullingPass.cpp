#include "FastJungle/renderer/pass/GpuCullingPass.hpp"

#include <algorithm>
#include <array>
#include <filesystem>

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    namespace {

        constexpr std::uint32_t THREAD_COUNT = 256;

        enum class RootParameter : std::uint32_t {
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
            BIN_CURSORS,
            COUNT,
        };

        [[nodiscard]]
        Microsoft::WRL::ComPtr<ID3D12PipelineState> create_pipeline(
            ID3D12Device* device,
            ID3D12RootSignature* root_signature,
            const std::filesystem::path& shader_path) {

            dx::Shader shader;
            shader.load(shader_path);

            auto description = dx::PSOUtils::default_compute_desc();
            description.pRootSignature = root_signature;
            description.CS = shader.get_bytecode();
            return dx::PSOUtils::create_compute(device, description);
        }

        void global_uav_barrier(ID3D12GraphicsCommandList* command_list) {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.UAV.pResource = nullptr;
            command_list->ResourceBarrier(1, &barrier);
        }

        [[nodiscard]]
        UINT dispatch_groups(std::uint32_t item_count) noexcept {
            return std::max(
                1u,
                (item_count + THREAD_COUNT - 1) / THREAD_COUNT);
        }

    } // namespace

    void GpuCullingPass::init(
        ID3D12Device* device,
        std::uint32_t indirect_draw_capacity_per_class) {

        indirect_draw_capacity_per_class_ =
            std::max(indirect_draw_capacity_per_class, 1u);

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
        root_builder.set_root_uav(RootParameter::BIN_CURSORS)
            .reg(5).vis_all().add();

        root_signature_ = root_builder.build(device);

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR};

        clear_pipeline_ = create_pipeline(
            device, root_signature_.Get(),
            shader_directory / "Clear.cs.dxil");
        count_pipeline_ = create_pipeline(
            device, root_signature_.Get(),
            shader_directory / "CullCount.cs.dxil");
        scan_pipeline_ = create_pipeline(
            device, root_signature_.Get(),
            shader_directory / "BinScan.cs.dxil");
        scatter_pipeline_ = create_pipeline(
            device, root_signature_.Get(),
            shader_directory / "CullScatter.cs.dxil");
        build_pipeline_ = create_pipeline(
            device, root_signature_.Get(),
            shader_directory / "BuildIndirect.cs.dxil");
    }

    void GpuCullingPass::record(
        dx::CommandContext& context,
        const data::DataPersistent& persistent,
        data::DataPerFrame& frame) const {

        auto* command_list = context.get();

        frame.indirect_gpu_draw.transition(
            command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        frame.indirect_gpu_draw_counts.transition(
            command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        frame.visible_instance.transition(
            command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        frame.bin_counts.transition(
            command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        frame.bin_offsets.transition(
            command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        frame.bin_cursors.transition(
            command_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        command_list->SetComputeRootSignature(root_signature_.Get());

        const std::array<std::uint32_t, 2> dispatch_constants{
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
            frame.bin_counts->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::BIN_OFFSETS),
            frame.bin_offsets->GetGPUVirtualAddress());
        command_list->SetComputeRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::BIN_CURSORS),
            frame.bin_cursors->GetGPUVirtualAddress());

        command_list->SetPipelineState(clear_pipeline_.Get());
        command_list->Dispatch(
            dispatch_groups(std::max(
                persistent.mesh_lod_count,
                data::Consts::RASTER_CLASS_CNT)),
            1,
            1);
        global_uav_barrier(command_list);

        command_list->SetPipelineState(count_pipeline_.Get());
        command_list->Dispatch(
            std::max(persistent.spatial_cluster_count, 1u),
            1,
            1);
        global_uav_barrier(command_list);

        command_list->SetPipelineState(scan_pipeline_.Get());
        command_list->Dispatch(1, 1, 1);
        global_uav_barrier(command_list);

        command_list->SetPipelineState(scatter_pipeline_.Get());
        command_list->Dispatch(
            std::max(persistent.spatial_cluster_count, 1u),
            1,
            1);
        global_uav_barrier(command_list);

        command_list->SetPipelineState(build_pipeline_.Get());
        command_list->Dispatch(
            dispatch_groups(persistent.mesh_lod_count),
            1,
            1);
        global_uav_barrier(command_list);

        // The CPU reads this only when this frame slot's fence has completed.
        // It is a temporary probe for final-LOD forest instances that could be
        // replaced by an impostor; it has no effect on culling or drawing.
        frame.bin_counts.transition(
            command_list, D3D12_RESOURCE_STATE_COPY_SOURCE);
        command_list->CopyBufferRegion(
            frame.bin_counts_readback.get(),
            0,
            frame.bin_counts.get(),
            0,
            static_cast<UINT64>(std::max(
                persistent.mesh_lod_count,
                1u)) * sizeof(std::uint32_t));

        frame.indirect_gpu_draw.transition(
            command_list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        frame.indirect_gpu_draw_counts.transition(
            command_list, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        frame.visible_instance.transition(
            command_list,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

} // namespace fjr::render
