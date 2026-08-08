#include "common/CullingWork.hlsli"

RWStructuredBuffer<uint> bin_counts : register(u3);

groupshared uint cluster_visible;

[numthreads(256, 1, 1)]
void main(
    uint3 group_id : SV_GroupID,
    uint3 group_thread_id : SV_GroupThreadID)
{
    const uint cluster_id = group_id.x;
    if (cluster_id >= spatial_cluster_count)
        return;

    const SpatialCluster cluster = spatial_clusters[cluster_id];

    if (group_thread_id.x == 0)
    {
        cluster_visible = SphereInFrustum(
            cam_normalized_frustum_planes,
            cluster.bounds_center,
            cluster.bounds_radius) ? 1 : 0;
    }
    GroupMemoryBarrierWithGroupSync();

    if (cluster_visible == 0 ||
        group_thread_id.x >= cluster.instance_count)
    {
        return;
    }

    const uint instance_id =
        cluster.instance_offset + group_thread_id.x;

    uint mesh_lod_id;
    if (ResolveVisibleMeshLod(cluster, instance_id, mesh_lod_id))
    {
        InterlockedAdd(bin_counts[mesh_lod_id], 1);
    }
}
