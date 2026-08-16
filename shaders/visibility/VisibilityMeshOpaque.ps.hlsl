#include "../common/ConstantMeshDispatch.hlsli"
#include "VisibilityCommon.hlsli"

struct MeshPrimitiveInput
{
    nointerpolation uint instance_id : TEXCOORD0;
    nointerpolation uint triangle_id : TEXCOORD1;
};

uint2 main(
    MeshPrimitiveInput input,
    bool front_face : SV_IsFrontFace) : SV_TARGET
{
    return PackVisibility(
        input.triangle_id,
        submesh_id,
        input.instance_id,
        front_face);
}
