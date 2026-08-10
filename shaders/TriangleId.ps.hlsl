#include "common/ForwardConstants.hlsli"

StructuredBuffer<Material> materials : register(t2);
Texture2D<float4> scene_textures[] : register(t3);
SamplerState material_sampler : register(s0);

struct TriangleIdPixelInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    nointerpolation uint instance_id : INSTANCE_ID;
};

uint hash_uint(uint value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float4 main(
    TriangleIdPixelInput input,
    uint primitive_id : SV_PrimitiveID) : SV_TARGET
{
    const Material material = materials[material_id];

    if (material.texture_opacity != INVALID_INDEX)
    {
        const float opacity = scene_textures[
            material.texture_opacity].Sample(
                material_sampler, input.uv).r;
        if (opacity <= 0.5f)
        {
            discard;
        }
    }

    uint triangle_id = primitive_id;
    triangle_id ^= input.instance_id * 0x9e3779b9u;
    triangle_id ^= material_id * 0x85ebca6bu;

    const uint hash = hash_uint(triangle_id);
    return float4(
        float3(
            hash & 0xffu,
            (hash >> 8u) & 0xffu,
            (hash >> 16u) & 0xffu) / 255.0f,
        1.0f);
}
