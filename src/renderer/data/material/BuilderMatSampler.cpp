#include "FastJungle/renderer/data/material/BuilderMatSampler.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdint>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/SamplerUtils.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data::mat {

    namespace {

        constexpr UINT MATERIAL_SAMPLER_COUNT = 2;
        constexpr UINT WRAP_SAMPLER_INDEX = 0;
        constexpr UINT CLAMP_SAMPLER_INDEX = 1;

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

        [[nodiscard]]
        std::array<uint32_t, MATERIAL_SAMPLER_COUNT>
        select_pipeline_samplers(
            const scene::StaticScene& scene) {

            std::array<uint32_t, MATERIAL_SAMPLER_COUNT> result{
                Consts::IND_ERR,
                Consts::IND_ERR,
            };

            for (uint32_t sampler_id = 0;
                sampler_id < scene.samplers.size();
                ++sampler_id) {

                const auto& sampler = scene.samplers[sampler_id];

                if (sampler.address_u ==
                    scene::StaticScene::EnumSamplerAddressMode::WRAP &&
                    sampler.address_v ==
                    scene::StaticScene::EnumSamplerAddressMode::WRAP) {

                    result[WRAP_SAMPLER_INDEX] = sampler_id;
                }

                if (sampler.address_u ==
                    scene::StaticScene::EnumSamplerAddressMode::CLAMP &&
                    sampler.address_v ==
                    scene::StaticScene::EnumSamplerAddressMode::CLAMP) {

                    result[CLAMP_SAMPLER_INDEX] = sampler_id;
                }
            }

            if (result[WRAP_SAMPLER_INDEX] == Consts::IND_ERR ||
                result[CLAMP_SAMPLER_INDEX] == Consts::IND_ERR) {

                log::Logger::g_logger << log::abrt(
                    "Scene must provide WRAP and CLAMP samplers.");
            }

            return result;
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

            const auto source_indices = select_pipeline_samplers(scene);
            output.samplers = heap_sampler.alloc(MATERIAL_SAMPLER_COUNT);

            for (UINT sampler_index = 0;
                sampler_index < MATERIAL_SAMPLER_COUNT;
                ++sampler_index) {

                const auto& source = scene.samplers[
                    source_indices[sampler_index]];
                auto description = make_default_sampler();
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

                dx::SamplerUtils::create(
                    device,
                    output.samplers.get_cpu(sampler_index),
                    description);
            }
        }

    } // namespace

    void BuilderMatSampler::build(
        DataPersistent& output,
        ID3D12Device* device,
        dx::DescriptorHeap& heap_sampler,
        const scene::StaticScene& scene) {

        create_samplers(output, device, heap_sampler, scene);
    }

} // namespace fjr::render::data::mat
