cbuffer nums : register(b0)
{
    uint total_cnt;
};

StructuredBuffer<uint> src : register(t0);
RWStructuredBuffer<uint> dst : register(u0);
RWByteAddressBuffer indirects : register(u1);

static const uint THREAD_CNT = 256;
static const uint GBUFFER_THREADS_PER_GROUP = 256;

groupshared uint shared_mem[THREAD_CNT];

[numthreads(THREAD_CNT, 1, 1)]
void main(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID)
{
    const uint idx = gid.x * THREAD_CNT + tid.x;

    shared_mem[tid.x] = idx < total_cnt ? src[idx] : 0;
    GroupMemoryBarrierWithGroupSync();

    for (uint offset = 1; offset < THREAD_CNT; offset <<= 1)
    {
        const uint addend = tid.x >= offset ? shared_mem[tid.x - offset] : 0;
        GroupMemoryBarrierWithGroupSync();
        if (tid.x >= offset)
        {
            shared_mem[tid.x] += addend;
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    uint command_offset = idx * 20;
    if (idx < total_cnt)
    {
        uint sum = shared_mem[tid.x];
        uint org = src[idx];
        dst[idx] = sum;
        
        uint pixel_offset = sum - org;
        uint pixel_count = org;
        
        uint dispatch_count =
            (pixel_count + GBUFFER_THREADS_PER_GROUP - 1) / GBUFFER_THREADS_PER_GROUP;
        
        indirects.Store2(command_offset, uint2(pixel_offset, pixel_count));
        indirects.Store3(command_offset + 8, uint3(dispatch_count, 1, 1));
    }
    else
    {
        indirects.Store2(command_offset, uint2(0, 0));
        indirects.Store3(command_offset + 8, uint3(0, 0, 0));
    }
}
