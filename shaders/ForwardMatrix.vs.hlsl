#include "ForwardCommon.hlsli"

ByteAddressBuffer instances : register(t0);

static const uint MATRIX_INSTANCE_SIZE = 64;

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PixelInput {
    float4 position : SV_POSITION;
    float3 world_position : TEXCOORD1;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

float4 transform_position_rows(float4 position, uint byte_offset) {
    const float4 row0 = asfloat(instances.Load4(byte_offset));
    const float4 row1 = asfloat(instances.Load4(byte_offset + 16));
    const float4 row2 = asfloat(instances.Load4(byte_offset + 32));
    const float4 row3 = asfloat(instances.Load4(byte_offset + 48));
    return
        position.x * row0 +
        position.y * row1 +
        position.z * row2 +
        position.w * row3;
}

float3 transform_normal_rows(float3 normal, uint byte_offset) {
    const float3 row0 = asfloat(instances.Load3(byte_offset));
    const float3 row1 = asfloat(instances.Load3(byte_offset + 16));
    const float3 row2 = asfloat(instances.Load3(byte_offset + 32));
    return
        normal.x * row0 +
        normal.y * row1 +
        normal.z * row2;
}

PixelInput main(VertexInput input, uint instance_id : SV_InstanceID) {
    PixelInput output;
    const uint byte_offset = instance_id * MATRIX_INSTANCE_SIZE;
    const float4 world_position = transform_position_rows(
        float4(input.position, 1.0),
        byte_offset);
    const float3 world_normal = transform_normal_rows(
        input.normal,
        byte_offset);

    output.position = mul(world_position, view_projection);
    output.world_position = world_position.xyz;
    output.normal = world_normal;
    output.uv = input.uv;
    return output;
}
