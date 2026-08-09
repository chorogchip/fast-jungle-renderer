#include "common/ForwardConstants.hlsli"

StructuredBuffer<uint> visible_instances : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct TriangleIdPixelInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    nointerpolation uint instance_id : INSTANCE_ID;
};

float3 RotateForwardVector(float3 value, float4 quaternion)
{
    const float3 twice_cross = 2.0f * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twice_cross +
        cross(quaternion.xyz, twice_cross);
}

TriangleIdPixelInput main(
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

    TriangleIdPixelInput output;
    output.position = mul(
        float4(world_position, 1.0f),
        cam_view_projection);
    output.uv = input.uv;
    output.instance_id = instance_id;
    return output;
}
