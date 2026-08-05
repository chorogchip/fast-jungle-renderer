#pragma once

#include <d3d12.h>
#include <array>
#include <vector>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/renderer/data/SceneResourcesTemp.hpp"

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
        };

        struct PointResources {
            dx::Buffer clusters;
            dx::Buffer batches;
            dx::Buffer definitions;
            dx::Buffer draw_templates;

            uint32_t cluster_count = 0;
            uint32_t batch_count = 0;
            uint32_t definition_count = 0;
            uint32_t draw_template_count = 0;
            uint32_t bin_count = 0;

            SceneResourcesTemp::PointIndirectLayout indirect_layout;
        };

        GeometryResources geometry;
        MaterialResources materials;
        InstanceResources instances;
        PointResources points;
    };

} // namespace fjr::render
