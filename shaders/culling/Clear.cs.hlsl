
#include "../common/ConstBufCamera.hlsli"
#include "../common/ConstantCulling.hlsli"

RWStructuredBuffer<uint> indirect_draw_counts : register(u1);
RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> bin_cursors : register(u5);

[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint tid = dtid.x;
    
    if (tid < raster_class_count)
    {
        indirect_draw_counts[tid] = 0;
    }
    
    if (tid < mesh_lod_count)
    {
        bin_counts[tid] = 0;
    }
    
    if (tid < mesh_lod_count)
    {
        bin_cursors[tid] = 0;
    }

}
