#include "CullingWork.hlsli"

RWStructuredBuffer<uint> visible_instances : register(u2);
RWStructuredBuffer<uint> bin_offsets : register(u4);
RWStructuredBuffer<uint> cluster_bin_bases : register(u5);
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
    const uint tid = group_thread_id.x;
    if (tid >= cluster.instance_count)
        return;

    const uint instance_id = cluster.instance_offset + tid;
    const uint packed_result = cull_results[instance_id];
    if (packed_result == CULL_RESULT_CULLED)
        return;

    const uint bucket = CullResultBucket(packed_result);
    const uint local_rank = CullResultLocalRank(packed_result);
    const Mesh mesh = meshes[cluster.mesh_id];
    const uint mesh_lod_id = CullBucketToMeshLod(mesh, bucket);
    const uint bin_base = cluster_bin_bases[
        cluster_id * CULL_RESERVATION_STRIDE + bucket];

    visible_instances[
        bin_offsets[mesh_lod_id] + bin_base + local_rank] = instance_id;
}
