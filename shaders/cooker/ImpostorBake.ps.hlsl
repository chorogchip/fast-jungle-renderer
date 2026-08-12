#include "../common/RenderConstants.hlsli"
#include "ImpostorBakeCommon.hlsli"

cbuffer BakeMaterial : register(b1)
{
    float4 base_color_opacity;
    uint base_color_texture;
    uint opacity_texture;
    uint base_color_channel;
    uint opacity_channel;
    float roughness_value;
    uint roughness_texture;
    uint roughness_channel;
    uint normal_texture;
    uint render_back_faces;
};

Texture2D<float4> source_textures[] : register(t0);
SamplerState source_sampler : register(s0);

struct PixelOutput
{
    float4 albedo_alpha : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float roughness : SV_TARGET2;
};

float select_channel(float4 value, uint channel)
{
    switch (channel)
    {
    case 1: return value.r;
    case 2: return value.g;
    case 3: return value.b;
    case 4: return value.a;
    default: return value.a;
    }
}

float3 surface_view_normal(ImpostorBakePixelInput input)
{
    const float3 normal = normalize(input.view_normal);
    if (normal_texture == INVALID_INDEX)
    {
        return normal;
    }

    const float3 position_dx = ddx(input.view_position);
    const float3 position_dy = ddy(input.view_position);
    const float2 uv_dx = ddx(input.uv);
    const float2 uv_dy = ddy(input.uv);
    const float3 position_dy_perp = cross(position_dy, normal);
    const float3 position_dx_perp = cross(normal, position_dx);
    const float3 tangent =
        position_dy_perp * uv_dx.x +
        position_dx_perp * uv_dy.x;
    const float3 bitangent =
        position_dy_perp * uv_dx.y +
        position_dx_perp * uv_dy.y;
    const float inverse_length = rsqrt(max(
        max(dot(tangent, tangent), dot(bitangent, bitangent)),
        1.0e-12f));
    const float2 tangent_xy = source_textures[
        normal_texture].Sample(source_sampler, input.uv).rg *
        2.0f - 1.0f;
    const float tangent_z = sqrt(saturate(
        1.0f - dot(tangent_xy, tangent_xy)));

    return normalize(
        tangent * (tangent_xy.x * inverse_length) +
        bitangent * (tangent_xy.y * inverse_length) +
        normal * tangent_z);
}

PixelOutput main(
    ImpostorBakePixelInput input,
    bool front_face : SV_IsFrontFace)
{
    if (!front_face && render_back_faces == 0u)
    {
        discard;
    }

    float4 albedo_alpha = base_color_opacity;
    if (base_color_texture != INVALID_INDEX)
    {
        const float4 sample = source_textures[base_color_texture].Sample(
            source_sampler,
            input.uv);
        albedo_alpha.rgb *= sample.rgb;
        if (base_color_channel != 5) // RGB does not carry opacity.
        {
            albedo_alpha.a *= select_channel(sample, base_color_channel);
        }
    }
    if (opacity_texture != INVALID_INDEX)
    {
        albedo_alpha.a *= select_channel(source_textures[opacity_texture].Sample(
            source_sampler,
            input.uv), opacity_channel);
    }
    if (albedo_alpha.a <= 0.5f)
    {
        discard;
    }

    PixelOutput output;
    output.albedo_alpha = albedo_alpha;
    float3 view_normal = surface_view_normal(input);
    if (!front_face)
    {
        // Runtime alpha rendering keeps both sides and flips the complete
        // mapped normal on a back face. Bake the same shading normal so the
        // impostor does not retain normals pointing away from its viewer.
        view_normal = -view_normal;
    }
    const float3 card_normal = float3(
        view_normal.x,
        -view_normal.y,
        -view_normal.z);
    output.normal = float4(card_normal * 0.5f + 0.5f, 1.0f);
    output.roughness = roughness_texture == INVALID_INDEX
        ? roughness_value
        : select_channel(source_textures[roughness_texture].Sample(
            source_sampler,
            input.uv), roughness_channel);
    return output;
}
