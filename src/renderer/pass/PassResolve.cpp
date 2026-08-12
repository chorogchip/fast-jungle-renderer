#include "FastJungle/renderer/pass/PassResolve.hpp"

#include <filesystem>
#include <utility>

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"

namespace fjr::render {

    namespace {

        enum class RootParameter : uint32_t {
            CAMERA,
            RESOURCES,
            TEXTURES,
            SAMPLERS,
            FRAME_BUFFER,
            COUNT,
        };

    } // namespace

    void PassResolve::init(
        ID3D12Device* device,
        PassResolveResources resources) {

        resources_ = std::move(resources);

        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(D3D12_ROOT_SIGNATURE_FLAG_NONE);

        root_builder.set_root_cbv(RootParameter::CAMERA)
            .reg(0).vis_all().add();
        root_builder.set_resource_table(RootParameter::RESOURCES)
            .srv().reg(0).count(10)
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE)
            .add_range()
            .vis_all().add();
        root_builder.set_resource_table(RootParameter::TEXTURES)
            .srv().reg(0).space(1)
            .count(resources_.textures.get_count())
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
            .add_range()
            .vis_all().add();
        root_builder.set_sampler_table(RootParameter::SAMPLERS)
            .sampler().reg(0).count(2).add_range()
            .vis_all().add();
        root_builder.set_resource_table(RootParameter::FRAME_BUFFER)
            .uav().reg(0).count(1).add_range()
            .vis_all().add();

        root_signature_ = root_builder.build(device);

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR};
        dx::Shader shader;
        shader.load(
            shader_directory / "visibility" / "ResolveOpaque.cs.dxil");

        auto description = dx::PSOUtils::default_compute_desc();
        description.pRootSignature = root_signature_.Get();
        description.CS = shader.get_bytecode();
        pipeline_state_ = dx::PSOUtils::create_compute(device, description);

    }

    void PassResolve::record(
        dx::CommandContext& context,
        uint32_t frame_index,
        UINT width,
        UINT height) {

        context->SetComputeRootSignature(root_signature_.Get());
        context->SetPipelineState(pipeline_state_.Get());

        context->SetComputeRootConstantBufferView(
            static_cast<UINT>(RootParameter::CAMERA),
            resources_.cameras[frame_index]);
        context->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::RESOURCES),
            resources_.inputs.get_gpu());
        context->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::TEXTURES),
            resources_.textures.get_gpu());
        context->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::SAMPLERS),
            resources_.samplers.get_gpu());
        context->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::FRAME_BUFFER),
            resources_.frame_buffer_uav.get_gpu());

        context.Dispatch<16, 16>(width, height);
    }

} // namespace fjr::render
