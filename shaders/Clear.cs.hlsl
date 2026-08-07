

cbuffer ClearConsts : register(b1)
{
    uint cnt_indirect_draw_counts;
    uint cnt_bin_counts;
    uint cnt_bin_cursors;
}

RWStructuredBuffer<uint> indirect_draw_counts : register(u1);
RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> bin_cursors : register(u5);

[numthreads(256, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint tid = dtid.x;
    
    if (tid < cnt_indirect_draw_counts)
    {
        indirect_draw_counts[tid] = 0;
    }
    
    if (tid < cnt_bin_counts)
    {
        bin_counts[tid] = 0;
    }
    
    if (tid < cnt_bin_cursors)
    {
        bin_cursors[tid] = 0;
    }

}