/*
#include "CullingWork.hlsli" RWStructuredBuffer<uint> visible_instances : register(u2); RWStructuredBuffer<uint> bin_offsets : register(u4); RWStructuredBuffer<uint> bin_cursors : register(u5); RWBuffer<uint> cull_results : register(u6); [numthreads(256, 1, 1)] void main( uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID) { const uint cluster_id = group_id.x; if (cluster_id >= spatial_cluster_count) return; const SpatialCluster cluster = spatial_clusters[cluster_id]; if (group_thread_id.x >= cluster.instance_count) return; const uint instance_id = cluster.instance_offset + group_thread_id.x; uint mesh_lod_id = cull_results[instance_id]; if (mesh_lod_id == CULL_RESULT_CULLED) return; uint offset_in_bin; InterlockedAdd(bin_cursors[mesh_lod_id], 1, offset_in_bin); visible_instances[bin_offsets[mesh_lod_id] + offset_in_bin] = instance_id; }
*/

#include "CullingWork.hlsli"

RWStructuredBuffer<uint> visible_instances : register(u2);
RWStructuredBuffer<uint> bin_offsets : register(u4);
RWStructuredBuffer<uint> bin_cursors : register(u5);
RWBuffer<uint> cull_results : register(u6);

static const uint HASH_SIZE = 256;
static const uint HASH_MASK = HASH_SIZE - 1;
static const uint EMPTY_KEY = 0xffffffffu;

groupshared uint hash_keys[HASH_SIZE];
groupshared uint hash_values[HASH_SIZE];

uint HashLod(uint lod)
{
    return (lod * 2654435761u) & HASH_MASK;
}

[numthreads(256, 1, 1)]
void main(
    uint3 group_id : SV_GroupID,
    uint3 group_thread_id : SV_GroupThreadID)
{
    const uint tid = group_thread_id.x;
    const uint cluster_id = group_id.x;

    if (cluster_id >= spatial_cluster_count)
        return;
    
    hash_keys[tid] = EMPTY_KEY;
    hash_values[tid] = 0;

    GroupMemoryBarrierWithGroupSync();

    const SpatialCluster cluster =
        spatial_clusters[cluster_id];

    const bool valid =
        tid < cluster.instance_count;

    uint instance_id = 0;
    uint mesh_lod_id = CULL_RESULT_CULLED;

    uint my_slot = 0;
    uint local_offset = 0;

    if (valid)
    {
        instance_id =
            cluster.instance_offset + tid;

        mesh_lod_id =
            cull_results[instance_id];

        if (mesh_lod_id != CULL_RESULT_CULLED)
        {
            uint slot = HashLod(mesh_lod_id);
            for (;;)
            {
                uint old_key;

                InterlockedCompareExchange(
                    hash_keys[slot],
                    EMPTY_KEY,
                    mesh_lod_id,
                    old_key);

                if (old_key == EMPTY_KEY ||
                    old_key == mesh_lod_id)
                {
                    my_slot = slot;
                    break;
                }

                slot = (slot + 1) & HASH_MASK;
            }
            
            InterlockedAdd(
                hash_values[my_slot],
                1,
                local_offset);
        }
    }

    GroupMemoryBarrierWithGroupSync();
    
    const uint key = hash_keys[tid];

    if (key != EMPTY_KEY)
    {
        const uint local_count =
            hash_values[tid];

        uint global_base;

        InterlockedAdd(
            bin_cursors[key],
            local_count,
            global_base);
        
        hash_values[tid] = global_base;
    }

    GroupMemoryBarrierWithGroupSync();

    if (valid &&
        mesh_lod_id != CULL_RESULT_CULLED)
    {
        visible_instances[
            bin_offsets[mesh_lod_id] +
            hash_values[my_slot] +
            local_offset
        ] = instance_id;
    }
}
