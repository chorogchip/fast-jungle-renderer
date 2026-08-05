#pragma once

#include <d3d12.h>
#include <array>
#include <vector>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/renderer/data/RenderTypesPointBatch.hpp"

namespace fjr::render::data {

    struct SceneResources {

        struct GeometryResources {
            dx::Buffer vertices;
            dx::Buffer indices;

            D3D12_VERTEX_BUFFER_VIEW vertex_view{};
            D3D12_INDEX_BUFFER_VIEW index_view{};
        };

        struct MaterialResources {
            dx::Buffer materials;
            dx::Buffer texture_bindings;

            std::vector<dx::Texture> textures;
            dx::DescAlloc texture_descriptors;
            dx::DescAlloc sampler_descriptors;
        };

        struct InstanceResources {
            dx::Buffer point_instances;
            dx::Buffer matrix_instances;

            dx::Buffer point_draw_constants;
            dx::Buffer matrix_draw_constants;

            uint32_t point_instance_count = 0;
            uint32_t matrix_instance_count = 0;
            uint32_t point_constant_count = 0;
            uint32_t matrix_constant_count = 0;
        };

        struct PointResources {
            dx::Buffer clusters;
            dx::Buffer mesh_batches;
            dx::Buffer definitions;
            dx::Buffer draw_templates;

            uint32_t cluster_count = 0;
            uint32_t mesh_batch_count = 0;
            uint32_t definition_count = 0;
            uint32_t draw_template_count = 0;
            uint32_t bin_count = 0;

            IndirectCommandLayout indirect_layout;
        };

        GeometryResources geometry;
        MaterialResources materials;
        InstanceResources instances;
        PointResources points;
    };

} // namespace fjr::render
