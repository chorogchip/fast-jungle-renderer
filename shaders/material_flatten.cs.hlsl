#include "..\common\donut_gbuffer_common.hlsli"

cbuffer nums : register(b0)
{
    uint screen_width;
    uint screen_height;
}

#ifndef MATERIAL_BIN_COUNT
#define MATERIAL_BIN_COUNT 256
#elif MATERIAL_BIN_COUNT != 256
#error Current material flatten implementation requires MATERIAL_BIN_COUNT == 256
#endif

#ifndef BLOCK_WID
#define BLOCK_WID 16
#endif

#define THREAD_CNT (BLOCK_WID * BLOCK_WID)
#define MATERIAL_OVERFLOW_BIN (MATERIAL_BIN_COUNT - 1)
#define MATERIAL_MAX_REAL_SHADER_ID (MATERIAL_BIN_COUNT - 2)

Texture2D<uint2> gVisibility : register(t0);
StructuredBuffer<GeometryInstanceData> gGeometryInstances : register(t1);
StructuredBuffer<SubmeshData> gSubmeshes : register(t2);
StructuredBuffer<MaterialData> gMaterials : register(t3);
StructuredBuffer<uint> gBinCountsPrefix : register(t4);

RWStructuredBuffer<uint> gBinCounts : register(u0);
RWStructuredBuffer<uint2> gFinalPos : register(u1);

groupshared uint sBinCounts[MATERIAL_BIN_COUNT];
groupshared uint sBinOffset[MATERIAL_BIN_COUNT];

uint ResolveShaderID(uint2 visibility)
{
    const uint geometry_instance_id = visibility.x - 1;
    const GeometryInstanceData geometry = gGeometryInstances[geometry_instance_id];
    const SubmeshData submesh = gSubmeshes[geometry.submeshID];
    const MaterialData material = gMaterials[submesh.materialID];
    return material.virtual_shader_id <= MATERIAL_MAX_REAL_SHADER_ID
        ? material.virtual_shader_id
        : MATERIAL_OVERFLOW_BIN;
}

[numthreads(BLOCK_WID, BLOCK_WID, 1)]
void kernel_material_flatten(
    uint3 pos : SV_DispatchThreadID,
    uint3 grp : SV_GroupThreadID)
{
    const uint group_index = grp.y * BLOCK_WID + grp.x;

    for (uint i = group_index; i < MATERIAL_BIN_COUNT; i += THREAD_CNT)
    {
        sBinCounts[i] = 0;
        sBinOffset[i] = i == 0 ? 0 : gBinCountsPrefix[i - 1];

    }
    GroupMemoryBarrierWithGroupSync();

    const uint2 pixel = pos.xy;
    uint shader_id = 0;
    uint final_offset = 0;
    bool valid = false;

    if (pixel.x < screen_width && pixel.y < screen_height)
    {
        const uint2 visibility = gVisibility.Load(int3(pixel, 0));
        if (visibility.x != 0)
        {
            shader_id = ResolveShaderID(visibility);
            uint offset_in_tile = 0;
            InterlockedAdd(sBinCounts[shader_id], 1, offset_in_tile);
            final_offset = sBinOffset[shader_id] + offset_in_tile;
            valid = true;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint i = group_index; i < MATERIAL_BIN_COUNT; i += THREAD_CNT)
    {
        const uint count = sBinCounts[i];
        uint offset_of_tile = 0;
        if (count != 0)
        {
            InterlockedAdd(gBinCounts[i], count, offset_of_tile);
            sBinOffset[i] = offset_of_tile;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (valid)
    {
        final_offset += sBinOffset[shader_id];
        gFinalPos[final_offset] = pixel;
    }
}
