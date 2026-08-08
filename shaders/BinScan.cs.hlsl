#include "common/CameraConstants.hlsli"

RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> bin_offsets : register(u4);

[numthreads(1, 1, 1)]
void main()
{
    uint offset = 0;

    for (uint mesh_lod_id = 0;
        mesh_lod_id < mesh_lod_count;
        ++mesh_lod_id)
    {
        const uint count = bin_counts[mesh_lod_id];
        bin_offsets[mesh_lod_id] = offset;
        offset += count;
    }
}
