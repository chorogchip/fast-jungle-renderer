#include "RenderData.hlsli"
#include "common/CameraConstants.hlsli"
#include "common/CullingDispatchConstants.hlsli"

StructuredBuffer<MeshLod> mesh_lods : register(t3);
StructuredBuffer<SubMesh> submeshes : register(t4);

RWStructuredBuffer<IndirectGPUDraw> indirect_draws : register(u0);
RWStructuredBuffer<uint> indirect_draw_counts : register(u1);
RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> bin_offsets : register(u4);

[numthreads(256, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    const uint mesh_lod_id = dispatch_thread_id.x;
    if (mesh_lod_id >= mesh_lod_count)
        return;

    const uint instance_count = bin_counts[mesh_lod_id];
    if (instance_count == 0)
        return;

    const MeshLod lod = mesh_lods[mesh_lod_id];

    for (uint local_submesh = 0;
        local_submesh < lod.submesh_count;
        ++local_submesh)
    {
        const SubMesh submesh =
            submeshes[lod.submesh_offset + local_submesh];

        if (submesh.raster_class >= raster_class_count)
            continue;

        uint command_in_class;
        InterlockedAdd(
            indirect_draw_counts[submesh.raster_class],
            1,
            command_in_class);

        if (command_in_class >= indirect_draw_capacity_per_class)
            continue;

        const uint command_id =
            submesh.raster_class * indirect_draw_capacity_per_class +
            command_in_class;

        IndirectGPUDraw draw;
        draw.visible_instance_offset = bin_offsets[mesh_lod_id];
        draw.material_id = submesh.material_id;
        draw.index_count_per_instance = submesh.index_count;
        draw.instance_count = instance_count;
        draw.start_index_location = submesh.index_offset;
        draw.base_vertex_location = submesh.base_vertex;
        draw.start_instance_location = 0;
        draw.padding = 0;

        indirect_draws[command_id] = draw;
    }
}
