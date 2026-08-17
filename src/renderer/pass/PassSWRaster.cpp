#include "FastJungle/renderer/pass/PassSWRaster.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <utility>

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    namespace {

        enum class RootParameter : uint32_t {
            CAMERA,
            BATCH_CONSTANTS,
            VISIBLE_INSTANCES,
            INSTANCES,
            VERTEX_DECODE_PARAMS,
            SUBMESHES,
            RASTER_CLUSTERS,
            RASTER_CLUSTER_VERTICES,
            RASTER_CLUSTER_TRIANGLES,
            OPAQUE_VERTICES,
            ALPHA_VERTICES,
            KEY,
            COUNT,
        };

    } // namespace

    void PassSWRaster::init(
        ID3D12Device* device,
        dx::DescriptorHeap& shader_heap,
        dx::DescriptorHeap& cpu_heap,
        Resources resources,
        uint32_t width,
        uint32_t height) {
        resources_ = std::move(resources);

        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(D3D12_ROOT_SIGNATURE_FLAG_NONE);
        root_builder.set_root_cbv(RootParameter::CAMERA)
            .reg(0).vis_all().add();
        root_builder.set_constants(RootParameter::BATCH_CONSTANTS)
            .reg(1)
            .count(data::DataPerFrame::SoftwareBatch::ROOT_CONST_CNT)
            .vis_all().add();
        root_builder.set_root_srv(RootParameter::VISIBLE_INSTANCES)
            .reg(0).vis_all().add();
        root_builder.set_root_srv(RootParameter::INSTANCES)
            .reg(1).vis_all().add();
        root_builder.set_root_srv(RootParameter::VERTEX_DECODE_PARAMS)
            .reg(2).vis_all().add();
        root_builder.set_root_srv(RootParameter::SUBMESHES)
            .reg(3).vis_all().add();
        root_builder.set_root_srv(RootParameter::RASTER_CLUSTERS)
            .reg(4).vis_all().add();
        root_builder.set_root_srv(RootParameter::RASTER_CLUSTER_VERTICES)
            .reg(5).vis_all().add();
        root_builder.set_root_srv(RootParameter::RASTER_CLUSTER_TRIANGLES)
            .reg(6).vis_all().add();
        root_builder.set_root_srv(RootParameter::OPAQUE_VERTICES)
            .reg(7).vis_all().add();
        root_builder.set_root_srv(RootParameter::ALPHA_VERTICES)
            .reg(8).vis_all().add();
        root_builder.set_resource_table(RootParameter::KEY)
            .uav().reg(0).count(1).add_range()
            .vis_all().add();
        root_signature_ = root_builder.build(device);

        const std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> arguments{
            D3D12_INDIRECT_ARGUMENT_DESC{
                .Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT,
                .Constant = {
                    .RootParameterIndex = static_cast<UINT>(
                        RootParameter::BATCH_CONSTANTS),
                    .DestOffsetIn32BitValues = 0,
                    .Num32BitValuesToSet = data::DataPerFrame::
                        SoftwareBatch::ROOT_CONST_CNT,
                },
            },
            D3D12_INDIRECT_ARGUMENT_DESC{
                .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH,
            },
        };
        const D3D12_COMMAND_SIGNATURE_DESC command_description{
            .ByteStride = sizeof(data::DataPerFrame::SoftwareBatch),
            .NumArgumentDescs = static_cast<UINT>(arguments.size()),
            .pArgumentDescs = arguments.data(),
            .NodeMask = 0,
        };
        dx::abort_failed(device->CreateCommandSignature(
            &command_description,
            root_signature_.Get(),
            IID_PPV_ARGS(command_signature_.ReleaseAndGetAddressOf())));

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR};
        dx::Shader shader;
        shader.load(
            shader_directory / "raster" / "SoftwareRaster.cs.dxil");
        auto description = dx::PSOUtils::default_compute_desc();
        description.pRootSignature = root_signature_.Get();
        description.CS = shader.get_bytecode();
        pso_ = dx::PSOUtils::create_compute(device, description);

        for (uint32_t frame = 0; frame < FRAME_COUNT; ++frame) {
            key_uavs_[frame] = shader_heap.alloc();
            key_clear_uavs_[frame] = cpu_heap.alloc();
        }
        resize(device, width, height);
    }

    void PassSWRaster::resize(
        ID3D12Device* device,
        uint32_t width,
        uint32_t height) {
        const UINT64 pixel_count = static_cast<UINT64>(width) * height;
        for (uint32_t frame = 0; frame < FRAME_COUNT; ++frame) {
            keys_[frame].reset();
            keys_[frame].init(
                device,
                (std::max)(pixel_count * sizeof(uint64_t), UINT64{8}),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            keys_[frame].create_raw_uav(
                device,
                key_uavs_[frame].get_cpu(),
                0,
                static_cast<UINT>((std::max)(
                    pixel_count * 2, UINT64{2})));
            keys_[frame].create_raw_uav(
                device,
                key_clear_uavs_[frame].get_cpu(),
                0,
                static_cast<UINT>((std::max)(
                    pixel_count * 2, UINT64{2})));
        }
    }

    void PassSWRaster::record(
        dx::CommandContext& context,
        uint32_t frame_index) {
        const auto& frame = resources_.frames[frame_index];
        auto& key = keys_[frame_index];

        context.transition(key, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        constexpr std::array<UINT, 4> clear_value{
            UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
        context->ClearUnorderedAccessViewUint(
            key_uavs_[frame_index].get_gpu(),
            key_clear_uavs_[frame_index].get_cpu(),
            key.get(),
            clear_value.data(),
            0,
            nullptr);
        context.uav_barrier(key);

        context->SetComputeRootSignature(root_signature_.Get());
        context->SetPipelineState(pso_.Get());
        context->SetComputeRootConstantBufferView(
            static_cast<UINT>(RootParameter::CAMERA),
            frame.camera);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::VISIBLE_INSTANCES),
            frame.visible_instances);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::INSTANCES),
            resources_.instances);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::VERTEX_DECODE_PARAMS),
            resources_.vertex_decode_params);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::SUBMESHES),
            resources_.submeshes);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::RASTER_CLUSTERS),
            resources_.raster_clusters);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::RASTER_CLUSTER_VERTICES),
            resources_.raster_cluster_vertices);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::RASTER_CLUSTER_TRIANGLES),
            resources_.raster_cluster_triangles);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::OPAQUE_VERTICES),
            resources_.opaque_vertices);
        context->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::ALPHA_VERTICES),
            resources_.alpha_vertices);
        context->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::KEY),
            key_uavs_[frame_index].get_gpu());

        context->ExecuteIndirect(
            command_signature_.Get(),
            data::Consts::SW_BATCH_CAPACITY,
            frame.batches,
            0,
            frame.batch_count,
            0);
        context.uav_barrier(key);
    }

} // namespace fjr::render
