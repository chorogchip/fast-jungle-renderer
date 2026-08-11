#include "../common/ConstBufCamera.hlsli"

cbuffer CullingDispatchConstants : register(b1)
{
    uint indirect_draw_capacity_per_class;
    uint raster_class_count;
};

RWStructuredBuffer<uint> indirect_draw_counts : register(u1);
RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> bin_cursors : register(u5);

[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    const uint tid = dtid.x;

    if (tid < raster_class_count)
    {
        indirect_draw_counts[tid] = 0;
    }

    if (tid < mesh_lod_count)
    {
        bin_counts[tid] = 0;
        bin_cursors[tid] = 0;
    }
}