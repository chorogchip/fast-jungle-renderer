cbuffer MaterialConstants : register(b1) {
    float4 base_color;
    float4 emissive_roughness;
    float4 surface;
    uint4 options;
};

Texture2D<float4> base_color_texture : register(t1);
Texture2D<float4> normal_texture : register(t2);
Texture2D<float4> roughness_texture : register(t3);
Texture2D<float4> opacity_texture : register(t4);
SamplerState material_samplers[4] : register(s0);

static const uint BASE_COLOR_TEXTURE = 1u << 0u;
static const uint NORMAL_TEXTURE = 1u << 1u;
static const uint ROUGHNESS_TEXTURE = 1u << 2u;
static const uint OPACITY_TEXTURE = 1u << 3u;

struct PixelInput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    bool front_face : SV_IsFrontFace;
};

float select_channel(float4 value, uint channel) {
    if (channel == 1u) {
        return value.g;
    }
    if (channel == 2u) {
        return value.b;
    }
    if (channel == 3u) {
        return value.a;
    }
    return value.r;
}

float4 main(PixelInput input) : SV_TARGET {
    // Blender-authored USD UVs use the lower-left image origin.
    const float2 uv = float2(input.uv.x, 1.0 - input.uv.y);
    float4 albedo = base_color;
    if ((options.x & BASE_COLOR_TEXTURE) != 0u) {
        albedo *= base_color_texture.Sample(material_samplers[0], uv);
    }

    float opacity = surface.y;
    if ((options.x & OPACITY_TEXTURE) != 0u) {
        opacity *= select_channel(
            opacity_texture.Sample(material_samplers[3], uv),
            options.z);
    }
    clip(opacity - surface.z);

    float3 normal = normalize(input.normal) * (input.front_face ? 1.0 : -1.0);
    if ((options.x & NORMAL_TEXTURE) != 0u) {
        const float3 tangent = normalize(
            input.tangent.xyz - normal * dot(normal, input.tangent.xyz));
        const float3 bitangent =
            normalize(cross(normal, tangent)) * input.tangent.w;
        const float3 tangent_normal =
            normal_texture.Sample(material_samplers[1], uv).xyz * 2.0 - 1.0;
        normal = normalize(
            tangent_normal.x * tangent +
            tangent_normal.y * bitangent +
            tangent_normal.z * normal);
    }

    float roughness = emissive_roughness.w;
    if ((options.x & ROUGHNESS_TEXTURE) != 0u) {
        roughness = select_channel(
            roughness_texture.Sample(material_samplers[2], uv),
            options.y);
    }
    roughness = saturate(roughness);

    const float3 light_direction = normalize(float3(0.45, -0.55, 0.75));
    const float diffuse = saturate(dot(normal, light_direction));
    const float3 view_direction = normalize(float3(0.0, -1.0, 0.65));
    const float3 half_vector = normalize(light_direction + view_direction);
    const float specular_power = lerp(128.0, 8.0, roughness);
    const float specular = pow(
        saturate(dot(normal, half_vector)),
        specular_power) * lerp(0.25, 0.03, roughness);
    const float metallic = saturate(surface.x);
    const float fresnel = pow(
        1.0 - saturate(abs(dot(normal, view_direction))),
        5.0);
    const float3 environment = float3(0.035, 0.055, 0.085) *
        lerp(0.35, 1.0, fresnel) * (1.0 - 0.5 * roughness);
    const float3 lit = albedo.rgb * (0.38 + 0.62 * diffuse) +
        lerp(specular.xxx, albedo.rgb * specular, metallic) +
        emissive_roughness.rgb + environment;
    const float3 display_color = pow(
        saturate(lit),
        1.0 / 2.2);
    return float4(display_color, 1.0);
}
