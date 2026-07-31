#pragma once

#include "FastJungle/dx12/DescriptorAllocator.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"

#include <d3d12.h>

namespace fjr::dx {

    class SamplerUtils {
    public:
        SamplerUtils() = delete;

        [[nodiscard]]
        static D3D12_GPU_DESCRIPTOR_HANDLE create(
            ID3D12Device* device,
            const DescriptorHeap& heap,
            const DescriptorAllocation& allocation,
            const D3D12_SAMPLER_DESC& description,
            UINT descriptor_offset = 0);

        [[nodiscard]]
        static D3D12_SAMPLER_DESC default_desc() noexcept;

        [[nodiscard]]
        static D3D12_SAMPLER_DESC point_wrap() noexcept;

        [[nodiscard]]
        static D3D12_SAMPLER_DESC point_clamp() noexcept;

        [[nodiscard]]
        static D3D12_SAMPLER_DESC linear_wrap() noexcept;

        [[nodiscard]]
        static D3D12_SAMPLER_DESC linear_clamp() noexcept;

        [[nodiscard]]
        static D3D12_SAMPLER_DESC anisotropic_wrap(
            UINT max_anisotropy = 16) noexcept;

        [[nodiscard]]
        static D3D12_SAMPLER_DESC anisotropic_clamp(
            UINT max_anisotropy = 16) noexcept;

        [[nodiscard]]
        static D3D12_SAMPLER_DESC comparison_linear_clamp(
            D3D12_COMPARISON_FUNC comparison =
            D3D12_COMPARISON_FUNC_LESS_EQUAL) noexcept;

        [[nodiscard]]
        static D3D12_STATIC_SAMPLER_DESC
            default_static_desc(
                UINT shader_register,
                UINT register_space = 0,
                D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL) noexcept;

        [[nodiscard]]
        static D3D12_STATIC_SAMPLER_DESC
            static_point_wrap(
                UINT shader_register,
                UINT register_space = 0,
                D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL) noexcept;

        [[nodiscard]]
        static D3D12_STATIC_SAMPLER_DESC
            static_point_clamp(
                UINT shader_register,
                UINT register_space = 0,
                D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL) noexcept;

        [[nodiscard]]
        static D3D12_STATIC_SAMPLER_DESC
            static_linear_wrap(
                UINT shader_register,
                UINT register_space = 0,
                D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL) noexcept;

        [[nodiscard]]
        static D3D12_STATIC_SAMPLER_DESC
            static_linear_clamp(
                UINT shader_register,
                UINT register_space = 0,
                D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL) noexcept;

        [[nodiscard]]
        static D3D12_STATIC_SAMPLER_DESC
            static_anisotropic_wrap(
                UINT shader_register,
                UINT register_space = 0,
                UINT max_anisotropy = 16,
                D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL) noexcept;

        [[nodiscard]]
        static D3D12_STATIC_SAMPLER_DESC
            static_anisotropic_clamp(
                UINT shader_register,
                UINT register_space = 0,
                UINT max_anisotropy = 16,
                D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL) noexcept;

        [[nodiscard]]
        static D3D12_STATIC_SAMPLER_DESC
            static_comparison_linear_clamp(
                UINT shader_register,
                UINT register_space = 0,
                D3D12_COMPARISON_FUNC comparison =
                D3D12_COMPARISON_FUNC_LESS_EQUAL,
                D3D12_SHADER_VISIBILITY visibility =
                D3D12_SHADER_VISIBILITY_ALL) noexcept;
    };

} // namespace fjr::dx