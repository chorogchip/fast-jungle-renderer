
#include "common/ForwardConstants.hlsli"

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

float3 RotateForwardVector(float3 value, float4 quaternion)
{
    const float3 twice_cross = 2.0f * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twice_cross +
        cross(quaternion.xyz, twice_cross);
}

ForwardPixelInput main(
    VertexInput input,
    uint local_instance_id : SV_InstanceID)
{
    const uint instance_id = visible_instances[
        visible_instance_offset + local_instance_id];
    const InstanceTransform instance = instances[instance_id];

    const float3 world_position =
        instance.position +
        RotateForwardVector(
            input.position * instance.scale,
            instance.rotation);

    const float3 inverse_scale =
        sign(instance.scale) /
        max(abs(instance.scale), 1.0e-8f);

    ForwardPixelInput output;
    output.position = mul(
        float4(world_position, 1.0f),
        cam_view_projection);
    output.world_normal = normalize(
        RotateForwardVector(
            input.normal * inverse_scale,
            instance.rotation));
    output.uv = input.uv;
    return output;
}
