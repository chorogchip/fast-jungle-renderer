#include "CullingWork.hlsli"

RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> cluster_bin_bases : register(u5);
RWBuffer<uint> cull_results : register(u6);

groupshared uint cluster_visible;
groupshared SpatialCluster group_cluster;
groupshared Mesh group_mesh;
groupshared uint local_counts[CULL_RESERVATION_STRIDE];

[numthreads(256, 1, 1)]
void main(
    uint3 group_id : SV_GroupID,
    uint3 group_thread_id : SV_GroupThreadID)
{
    const uint cluster_id = group_id.x;
    const uint tid = group_thread_id.x;

    if (cluster_id >= spatial_cluster_count)
        return;

    if (tid == 0)
    {
        group_cluster = spatial_clusters[cluster_id];
        group_mesh = meshes[group_cluster.mesh_id];
        cluster_visible = SphereInFrustum(
            cam_normalized_frustum_planes,
            group_cluster.bounds_center,
            group_cluster.bounds_radius) ? 1 : 0;
    }

    if (tid < CULL_RESERVATION_STRIDE)
        local_counts[tid] = 0;

    GroupMemoryBarrierWithGroupSync();

    const SpatialCluster cluster = group_cluster;
    const Mesh mesh = group_mesh;
    const bool valid = tid < cluster.instance_count;
    const uint instance_id = cluster.instance_offset + tid;
    uint packed_result = CULL_RESULT_CULLED;

    if (valid && cluster_visible)
    {
        uint mesh_lod_id;
        if (ResolveVisibleMeshLod(
            cluster, mesh, instance_id, mesh_lod_id))
        {
            uint bucket;
            if (TryGetCullBucket(mesh, mesh_lod_id, bucket))
            {
                uint local_rank;
                InterlockedAdd(
                    local_counts[bucket],
                    1,
                    local_rank);
                packed_result = PackCullResult(bucket, local_rank);
            }
        }
    }

    if (valid)
        cull_results[instance_id] = packed_result;

    GroupMemoryBarrierWithGroupSync();

    if (tid < CULL_BUCKET_COUNT)
    {
        const uint local_count = local_counts[tid];
        if (local_count != 0)
        {
            const uint mesh_lod_id = CullBucketToMeshLod(mesh, tid);
            uint bin_base;
            InterlockedAdd(
                bin_counts[mesh_lod_id],
                local_count,
                bin_base);
            cluster_bin_bases[
                cluster_id * CULL_RESERVATION_STRIDE + tid] = bin_base;
        }
    }
}
