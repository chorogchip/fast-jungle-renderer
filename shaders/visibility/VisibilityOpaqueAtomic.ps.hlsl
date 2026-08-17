#include "../common/ConstBufCamera.hlsli"
#include "../common/ConstantDraw.hlsli"

struct OpaqueAtomicVisibilityVertexOutput
{
    float4 position : SV_Position;
    nointerpolation uint local_instance_id : TEXCOORD0;
};

RWByteAddressBuffer visibility_key : register(u0);

[earlydepthstencil]
void main(
    OpaqueAtomicVisibilityVertexOutput input,
    uint triangle_id : SV_PrimitiveID)
{
    const uint local_primitive =
        input.local_instance_id * triangle_count + triangle_id;
    const uint primitive_id =
        (visibility_batch_id << 24u) | local_primitive;
    const uint64_t key =
        (uint64_t(asuint(input.position.z)) << 32u) | primitive_id;
    const uint2 pixel = uint2(input.position.xy);
    const uint byte_offset =
        (pixel.y * cam_pixel_width + pixel.x) * 8u;
    visibility_key.InterlockedMin64(byte_offset, key);
}
