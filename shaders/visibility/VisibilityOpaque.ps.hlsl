#include "../common/ConstantDraw.hlsli"
#include "VisibilityCommon.hlsli"

uint2 main(
    OpaqueVisibilityVertexOutput input,
    uint triangle_id : SV_PrimitiveID,
    bool front_face : SV_IsFrontFace) : SV_TARGET
{
    return PackVisibility(
        triangle_id,
        submesh_id,
        input.instance_id,
        front_face);
}
