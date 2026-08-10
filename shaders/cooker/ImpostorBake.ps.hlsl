static const uint INVALID_INDEX = 0xffffffffu;

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
};

Texture2D<float4> source_textures[] : register(t0);
SamplerState source_sampler : register(s0);

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 view_normal : NORMAL;
    float2 uv : TEXCOORD0;
    float3 view_position : TEXCOORD1;
};

struct PixelOutput
{
    float4 albedo_alpha : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float depth : SV_TARGET2;
    float roughness : SV_TARGET3;
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

float3 surface_view_normal(PixelInput input)
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

PixelOutput main(PixelInput input)
{
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
    const float3 view_normal = surface_view_normal(input);
    const float3 card_normal = float3(
        view_normal.x,
        -view_normal.y,
        -view_normal.z);
    output.normal = float4(card_normal * 0.5f + 0.5f, 1.0f);
    output.depth = input.view_position.z;
    output.roughness = roughness_texture == INVALID_INDEX
        ? roughness_value
        : select_channel(source_textures[roughness_texture].Sample(
            source_sampler,
            input.uv), roughness_channel);
    return output;
}
