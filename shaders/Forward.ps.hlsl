#include "common/ForwardConstants.hlsli"

StructuredBuffer<Material> materials : register(t2);
Texture2D<float4> scene_textures[] : register(t3);
SamplerState scene_samplers[] : register(s0);

float4 main(ForwardPixelInput input) : SV_TARGET
{
    const Material material = materials[material_id];

    float3 albedo = material.base_color;
    float opacity = 1.0f;

    if (material.texture_basecolor != INVALID_INDEX)
    {
        const float4 sample = scene_textures[
            material.texture_basecolor].Sample(
                scene_samplers[0],
                input.uv);
        albedo *= sample.rgb;
        opacity *= sample.a;
    }

    if (material.texture_opacity != INVALID_INDEX)
    {
        opacity *= scene_textures[
            material.texture_opacity].Sample(
                scene_samplers[0],
                input.uv).r;
    }

    clip(opacity - 0.5f);

    const float3 normal = normalize(input.world_normal);
    const float3 light_direction =
        normalize(float3(0.45f, 0.75f, -0.55f));
    const float diffuse = saturate(dot(normal, light_direction));
    const float lighting = 0.12f + 0.88f * diffuse;

    return float4(albedo * lighting, 1.0f);
}
