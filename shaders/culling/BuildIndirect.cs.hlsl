#include "../common/RenderData.hlsli"
#include "../common/ConstBufCamera.hlsli"
#include "CullingDispatchConstants.hlsli"

StructuredBuffer<MeshLod> mesh_lods : register(t3);
StructuredBuffer<SubMesh> submeshes : register(t4);

RWStructuredBuffer<IndirectGPUDraw> indirect_draws : register(u0);
RWStructuredBuffer<uint> indirect_draw_counts : register(u1);
RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> bin_offsets : register(u4);
RWStructuredBuffer<SoftwareBatch> software_batches : register(u7);
RWStructuredBuffer<uint> software_batch_count : register(u8);

static const uint RASTER_CLASS_OPAQUE = 2;
static const uint RASTER_CLASS_ALPHA = 4;
static const uint SOFTWARE_RASTER_OPAQUE = 1u << 0u;
static const uint SOFTWARE_RASTER_ALPHA = 1u << 1u;
static const uint SOFTWARE_LOCAL_WORK_CAPACITY = 1u << 17u;
static const uint SOFTWARE_BATCH_CAPACITY = 1u << 8u;
static const uint MAX_DISPATCH_DIMENSION = 65535;

void WriteSoftwareBatch(
    uint visible_instance_offset,
    uint instance_count,
    uint cluster_offset,
    uint cluster_count,
    uint submesh_id)
{
    uint batch_id;
    InterlockedAdd(software_batch_count[0], 1, batch_id);
    if (batch_id >= SOFTWARE_BATCH_CAPACITY)
        return;

    SoftwareBatch batch;
    batch.batch_id = batch_id;
    batch.visible_instance_offset = visible_instance_offset;
    batch.cluster_offset = cluster_offset;
    batch.cluster_count = cluster_count;
    batch.submesh_id = submesh_id;
    batch.thread_group_count_x = cluster_count;
    batch.thread_group_count_y = instance_count;
    batch.thread_group_count_z = 1;
    software_batches[batch_id] = batch;
}

void BuildSoftwareBatches(
    uint visible_instance_offset,
    uint instance_count,
    SubMesh submesh,
    uint submesh_id)
{
    for (uint cluster_begin = 0;
        cluster_begin < submesh.raster_cluster_count;
        cluster_begin += MAX_DISPATCH_DIMENSION)
    {
        const uint cluster_count = min(
            MAX_DISPATCH_DIMENSION,
            submesh.raster_cluster_count - cluster_begin);
        const uint instances_per_batch =
            min(
                MAX_DISPATCH_DIMENSION,
                SOFTWARE_LOCAL_WORK_CAPACITY / cluster_count);
        for (uint instance_begin = 0;
            instance_begin < instance_count;
            instance_begin += instances_per_batch)
        {
            WriteSoftwareBatch(
                visible_instance_offset + instance_begin,
                min(instances_per_batch, instance_count - instance_begin),
                submesh.raster_cluster_offset + cluster_begin,
                cluster_count,
                submesh_id);
        }
    }
}

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

    for (uint sm = 0; sm < lod.submesh_count; ++sm)
    {
        uint submesh_id = lod.submesh_offset + sm;
        const SubMesh submesh = submeshes[submesh_id];

        if (submesh.raster_class >= raster_class_count)
            continue;

        const uint software_raster_flag =
            submesh.raster_class == RASTER_CLASS_OPAQUE
                ? SOFTWARE_RASTER_OPAQUE
                : submesh.raster_class == RASTER_CLASS_ALPHA
                    ? SOFTWARE_RASTER_ALPHA
                    : 0;
        const bool software_raster =
            (lod.software_raster_mask & software_raster_flag) != 0 &&
            submesh.raster_cluster_count != 0;
        if (software_raster)
        {
            BuildSoftwareBatches(
                bin_offsets[mesh_lod_id],
                instance_count,
                submesh,
                submesh_id);
            continue;
        }

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
        draw.submesh_id = submesh_id;
        draw.index_count_per_instance = submesh.index_count;
        draw.instance_count = instance_count;
        draw.start_index_location = submesh.index_offset;
        draw.base_vertex_location = submesh.base_vertex;
        draw.start_instance_location = 0;

        indirect_draws[command_id] = draw;
    }
}
