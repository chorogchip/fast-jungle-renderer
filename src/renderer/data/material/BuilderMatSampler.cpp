#include "FastJungle/renderer/data/material/BuilderMatSampler.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdint>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/SamplerUtils.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data::mat {

    namespace {

        constexpr UINT MATERIAL_SAMPLER_COUNT = 2;

        [[nodiscard]]
        D3D12_SAMPLER_DESC make_default_sampler() noexcept {

            D3D12_SAMPLER_DESC description{};
            description.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            description.MaxAnisotropy = 1;
            description.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            description.MinLOD = 0.0f;
            description.MaxLOD = FLT_MAX;

            return description;
        }

        void create_samplers(
            DataPersistent& output,
            ID3D12Device* device,
            dx::DescriptorHeap& heap_sampler,
            const scene::StaticScene& scene) {

            if (scene.samplers.size() > MATERIAL_SAMPLER_COUNT) {
                log::Logger::g_logger << log::abrt(
                    "Scene uses more than two samplers.");
            }

            output.samplers = heap_sampler.alloc(MATERIAL_SAMPLER_COUNT);

            for (UINT sampler_id = 0;
                sampler_id < MATERIAL_SAMPLER_COUNT;
                ++sampler_id) {

                auto description = make_default_sampler();

                if (sampler_id < scene.samplers.size()) {
                    const auto& source = scene.samplers[sampler_id];

                    description.Filter = static_cast<D3D12_FILTER>(
                        source.filter);
                    description.AddressU =
                        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(
                            source.address_u);
                    description.AddressV =
                        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(
                            source.address_v);
                    description.AddressW =
                        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(
                            source.address_w);
                    description.MaxAnisotropy = std::clamp(
                        source.max_anisotropy,
                        1u,
                        16u);
                }

                dx::SamplerUtils::create(
                    device,
                    output.samplers.get_cpu(sampler_id),
                    description);
            }
        }

        void select_pipeline_samplers(
            DataPersistent& output,
            const scene::StaticScene& scene) {

            output.wrap_sampler = Consts::IND_ERR;
            output.clamp_sampler = Consts::IND_ERR;

            for (uint32_t sampler_id = 0;
                sampler_id < scene.samplers.size();
                ++sampler_id) {

                const auto& sampler = scene.samplers[sampler_id];

                if (sampler.address_u ==
                    scene::StaticScene::EnumSamplerAddressMode::WRAP &&
                    sampler.address_v ==
                    scene::StaticScene::EnumSamplerAddressMode::WRAP) {

                    output.wrap_sampler = sampler_id;
                }

                if (sampler.address_u ==
                    scene::StaticScene::EnumSamplerAddressMode::CLAMP &&
                    sampler.address_v ==
                    scene::StaticScene::EnumSamplerAddressMode::CLAMP) {

                    output.clamp_sampler = sampler_id;
                }
            }

            if (output.wrap_sampler == Consts::IND_ERR ||
                output.clamp_sampler == Consts::IND_ERR) {

                log::Logger::g_logger << log::abrt(
                    "Scene must provide WRAP and CLAMP samplers.");
            }
        }

    } // namespace

    void BuilderMatSampler::build(
        DataPersistent& output,
        ID3D12Device* device,
        dx::DescriptorHeap& heap_sampler,
        const scene::StaticScene& scene) {

        create_samplers(output, device, heap_sampler, scene);
        select_pipeline_samplers(output, scene);
    }

} // namespace fjr::render::data::mat
