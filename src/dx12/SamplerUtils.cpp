#include "FastJungle/dx12/SamplerUtils.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdlib>

namespace fjr::dx {

    namespace {

        D3D12_SAMPLER_DESC make_sampler(
            D3D12_FILTER filter,
            D3D12_TEXTURE_ADDRESS_MODE address_mode,
            UINT max_anisotropy = 1,
            D3D12_COMPARISON_FUNC comparison =
            D3D12_COMPARISON_FUNC_ALWAYS) noexcept {

            D3D12_SAMPLER_DESC description{};
            description.Filter = filter;
            description.AddressU = address_mode;
            description.AddressV = address_mode;
            description.AddressW = address_mode;
            description.MipLODBias = 0.0f;
            description.MaxAnisotropy =
                std::clamp(max_anisotropy, 1u, 16u);
            description.ComparisonFunc = comparison;
            description.BorderColor[0] = 0.0f;
            description.BorderColor[1] = 0.0f;
            description.BorderColor[2] = 0.0f;
            description.BorderColor[3] = 0.0f;
            description.MinLOD = 0.0f;
            description.MaxLOD = FLT_MAX;

            return description;
        }

        D3D12_STATIC_SAMPLER_DESC make_static_sampler(
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility,
            D3D12_FILTER filter,
            D3D12_TEXTURE_ADDRESS_MODE address_mode,
            UINT max_anisotropy = 1,
            D3D12_COMPARISON_FUNC comparison =
            D3D12_COMPARISON_FUNC_ALWAYS) noexcept {

            D3D12_STATIC_SAMPLER_DESC description{};
            description.Filter = filter;
            description.AddressU = address_mode;
            description.AddressV = address_mode;
            description.AddressW = address_mode;
            description.MipLODBias = 0.0f;
            description.MaxAnisotropy =
                std::clamp(max_anisotropy, 1u, 16u);
            description.ComparisonFunc = comparison;
            description.BorderColor =
                D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
            description.MinLOD = 0.0f;
            description.MaxLOD = FLT_MAX;
            description.ShaderRegister = shader_register;
            description.RegisterSpace = register_space;
            description.ShaderVisibility = visibility;

            return description;
        }

    } // namespace

    D3D12_GPU_DESCRIPTOR_HANDLE SamplerUtils::create(
        ID3D12Device* device,
        const DescriptorHeap& heap,
        const DescriptorAllocation& allocation,
        const D3D12_SAMPLER_DESC& description,
        UINT descriptor_offset) {

        if (descriptor_offset >= allocation.get_count()) {
            std::abort();
        }

        device->CreateSampler(
            &description,
            heap.get_cpu_handle(
                allocation,
                descriptor_offset));

        return heap.get_gpu_handle(
            allocation,
            descriptor_offset);
    }

    D3D12_SAMPLER_DESC
        SamplerUtils::default_desc() noexcept {
        return linear_wrap();
    }

    D3D12_SAMPLER_DESC
        SamplerUtils::point_wrap() noexcept {
        return make_sampler(
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }

    D3D12_SAMPLER_DESC
        SamplerUtils::point_clamp() noexcept {
        return make_sampler(
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    }

    D3D12_SAMPLER_DESC
        SamplerUtils::linear_wrap() noexcept {
        return make_sampler(
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }

    D3D12_SAMPLER_DESC
        SamplerUtils::linear_clamp() noexcept {
        return make_sampler(
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    }

    D3D12_SAMPLER_DESC
        SamplerUtils::anisotropic_wrap(
            UINT max_anisotropy) noexcept {

        return make_sampler(
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            max_anisotropy);
    }

    D3D12_SAMPLER_DESC
        SamplerUtils::anisotropic_clamp(
            UINT max_anisotropy) noexcept {

        return make_sampler(
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            max_anisotropy);
    }

    D3D12_SAMPLER_DESC
        SamplerUtils::comparison_linear_clamp(
            D3D12_COMPARISON_FUNC comparison) noexcept {

        return make_sampler(
            D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            1,
            comparison);
    }

    D3D12_STATIC_SAMPLER_DESC
        SamplerUtils::default_static_desc(
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility) noexcept {

        return static_linear_wrap(
            shader_register,
            register_space,
            visibility);
    }

    D3D12_STATIC_SAMPLER_DESC
        SamplerUtils::static_point_wrap(
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility) noexcept {

        return make_static_sampler(
            shader_register,
            register_space,
            visibility,
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }

    D3D12_STATIC_SAMPLER_DESC
        SamplerUtils::static_point_clamp(
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility) noexcept {

        return make_static_sampler(
            shader_register,
            register_space,
            visibility,
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    }

    D3D12_STATIC_SAMPLER_DESC
        SamplerUtils::static_linear_wrap(
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility) noexcept {

        return make_static_sampler(
            shader_register,
            register_space,
            visibility,
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }

    D3D12_STATIC_SAMPLER_DESC
        SamplerUtils::static_linear_clamp(
            UINT shader_register,
            UINT register_space,
            D3D12_SHADER_VISIBILITY visibility) noexcept {

        return make_static_sampler(
            shader_register,
            register_space,
            visibility,
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    }

    D3D12_STATIC_SAMPLER_DESC
        SamplerUtils::static_anisotropic_wrap(
            UINT shader_register,
            UINT register_space,
            UINT max_anisotropy,
            D3D12_SHADER_VISIBILITY visibility) noexcept {

        return make_static_sampler(
            shader_register,
            register_space,
            visibility,
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            max_anisotropy);
    }

    D3D12_STATIC_SAMPLER_DESC
        SamplerUtils::static_anisotropic_clamp(
            UINT shader_register,
            UINT register_space,
            UINT max_anisotropy,
            D3D12_SHADER_VISIBILITY visibility) noexcept {

        return make_static_sampler(
            shader_register,
            register_space,
            visibility,
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            max_anisotropy);
    }

    D3D12_STATIC_SAMPLER_DESC
        SamplerUtils::static_comparison_linear_clamp(
            UINT shader_register,
            UINT register_space,
            D3D12_COMPARISON_FUNC comparison,
            D3D12_SHADER_VISIBILITY visibility) noexcept {

        return make_static_sampler(
            shader_register,
            register_space,
            visibility,
            D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            1,
            comparison);
    }

} // namespace fjr::dx