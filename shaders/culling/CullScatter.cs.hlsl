#include "CullingWork.hlsli"

RWStructuredBuffer<uint> visible_instances : register(u2);
RWStructuredBuffer<uint> bin_offsets : register(u4);
RWStructuredBuffer<uint> bin_cursors : register(u5);
RWBuffer<uint> cull_results : register(u6);


[numthreads(256, 1, 1)]
void main(
    uint3 group_id : SV_GroupID,
    uint3 group_thread_id : SV_GroupThreadID)
{
    const uint cluster_id = group_id.x;
    if (cluster_id >= spatial_cluster_count)
        return;

    const SpatialCluster cluster = spatial_clusters[cluster_id];

    if (group_thread_id.x >= cluster.instance_count)
        return;
    
    const uint instance_id = cluster.instance_offset + group_thread_id.x;

    uint mesh_lod_id = cull_results[instance_id];
    
    if (mesh_lod_id == CULL_RESULT_CULLED)
        return;
    
    uint offset_in_bin;
    InterlockedAdd(bin_cursors[mesh_lod_id], 1, offset_in_bin);
    visible_instances[
        bin_offsets[mesh_lod_id] + offset_in_bin] = instance_id;
}