#include "../common/ConstBufCamera.hlsli"

RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> bin_offsets : register(u4);

static const uint THREAD_CNT = 1024;
static const uint ELEMENT_CNT = 2048;

groupshared uint shared_mem[ELEMENT_CNT];

[numthreads(THREAD_CNT, 1, 1)]
void main(uint3 tid : SV_GroupThreadID)
{
    const uint i0 = tid.x;
    const uint i1 = tid.x + THREAD_CNT;

    shared_mem[i0] =
        i0 < mesh_lod_count ? bin_counts[i0] : 0;

    shared_mem[i1] =
        i1 < mesh_lod_count ? bin_counts[i1] : 0;

    GroupMemoryBarrierWithGroupSync();
    
    for (uint offset = 1; offset < ELEMENT_CNT; offset <<= 1)
    {
        const uint add0 =
            i0 >= offset ? shared_mem[i0 - offset] : 0;

        const uint add1 =
            i1 >= offset ? shared_mem[i1 - offset] : 0;

        GroupMemoryBarrierWithGroupSync();

        shared_mem[i0] += add0;
        shared_mem[i1] += add1;

        GroupMemoryBarrierWithGroupSync();
    }
    
    if (i0 < mesh_lod_count)
        bin_offsets[i0] = shared_mem[i0] - bin_counts[i0];

    if (i1 < mesh_lod_count)
        bin_offsets[i1] = shared_mem[i1] - bin_counts[i1];
}