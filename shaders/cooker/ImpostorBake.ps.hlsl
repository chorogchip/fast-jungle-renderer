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
    uint padding;
};

Texture2D<float4> source_textures[] : register(t0);
SamplerState source_sampler : register(s0);

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 view_normal : NORMAL;
    float2 uv : TEXCOORD0;
    float view_depth : TEXCOORD1;
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
    clip(albedo_alpha.a - 0.5f);

    PixelOutput output;
    output.albedo_alpha = albedo_alpha;
    output.normal = float4(normalize(input.view_normal) * 0.5f + 0.5f, 1.0f);
    output.depth = input.view_depth;
    output.roughness = roughness_texture == INVALID_INDEX
        ? roughness_value
        : select_channel(source_textures[roughness_texture].Sample(
            source_sampler,
            input.uv), roughness_channel);
    return output;
}
