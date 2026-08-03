#pragma once

#include "FastJungle/renderer/SceneResources.hpp"

#include <d3d12.h>

#include <cstdint>
#include <span>

namespace fjr::render {

    // These are command-recording views, not resource owners. RendererMain
    // resolves SceneResources into GPU addresses and descriptor handles before
    // handing a view to a pass.
    struct GeometryPassView {
        D3D12_CPU_DESCRIPTOR_HANDLE render_target{};
        D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil{};

        std::uint32_t width = 0;
        std::uint32_t height = 0;

        D3D12_GPU_VIRTUAL_ADDRESS camera_constants = 0;
        D3D12_GPU_VIRTUAL_ADDRESS point_transform_constants = 0;
        D3D12_GPU_VIRTUAL_ADDRESS matrix_transform_constants = 0;
        D3D12_GPU_VIRTUAL_ADDRESS point_instances = 0;
        D3D12_GPU_VIRTUAL_ADDRESS matrix_instances = 0;

        D3D12_VERTEX_BUFFER_VIEW vertices{};
        D3D12_INDEX_BUFFER_VIEW indices{};
        std::span<const SceneResources::DrawItem> draws;
    };

    struct ForwardPassView : GeometryPassView {
        D3D12_GPU_VIRTUAL_ADDRESS materials = 0;
        D3D12_GPU_VIRTUAL_ADDRESS texture_bindings = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE textures{};
        D3D12_GPU_DESCRIPTOR_HANDLE samplers{};
    };

    struct VisibilityPassView : GeometryPassView {};

    struct VisibilityResolvePassView {
        D3D12_CPU_DESCRIPTOR_HANDLE render_target{};

        std::uint32_t width = 0;
        std::uint32_t height = 0;

        D3D12_GPU_DESCRIPTOR_HANDLE visibility{};
        D3D12_GPU_VIRTUAL_ADDRESS draws = 0;
        D3D12_GPU_VIRTUAL_ADDRESS materials = 0;
    };

} // namespace fjr::render
