#include "../common/ConstantDraw.hlsli"

struct VS_output
{
    float4 position : SV_Position;
    nointerpolation uint instance_id;
};

uint2 main(VS_output input, uint triangle_id : SV_PrimitiveID) : SV_TARGET0
{
    uint lower = triangle_id | ((submesh_id & 0x3ffu) << 22);
    uint upper = (submesh_id >> 10) | (input.instance_id << 1);
    return uint2(lower, upper);
}
