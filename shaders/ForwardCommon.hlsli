#include "common/ForwardConstants.hlsli"

StructuredBuffer<Material> materials : register(t2);
Texture2D<float4> scene_textures[] : register(t3);
SamplerState material_sampler : register(s0);
SamplerState environment_sampler : register(s1);

static const float PI = 3.14159265358979323846f;

float distribution_ggx(float n_dot_h, float roughness)
{
    const float alpha = roughness * roughness;
    const float alpha_squared = alpha * alpha;
    const float denominator = n_dot_h * n_dot_h *
        (alpha_squared - 1.0f) + 1.0f;
    return alpha_squared / max(PI * denominator * denominator, 1.0e-6f);
}

float visibility_smith_ggx(float n_dot_v, float n_dot_l, float roughness)
{
    const float k = (roughness + 1.0f) * (roughness + 1.0f) * 0.125f;
    const float view = n_dot_v / max(n_dot_v * (1.0f - k) + k, 1.0e-4f);
    const float light = n_dot_l / max(n_dot_l * (1.0f - k) + k, 1.0e-4f);
    return view * light;
}

float3 fresnel_schlick(float v_dot_h)
{
    return 0.04f.xxx + 0.96f.xxx * pow(1.0f - v_dot_h, 5.0f);
}

float2 environment_uv(float3 direction)
{
    direction = normalize(direction);
    return float2(
        atan2(direction.x, direction.z) / (2.0f * PI) + 0.5f,
        acos(clamp(direction.y, -1.0f, 1.0f)) / PI);
}

float3 normal_from_map(ForwardPixelInput input, Material material)
{
    const float3 normal = normalize(input.world_normal);
    if (material.texture_normal == INVALID_INDEX)
    {
        return normal;
    }

    const float3 position_dx = ddx(input.world_position);
    const float3 position_dy = ddy(input.world_position);
    const float2 uv_dx = ddx(input.uv);
    const float2 uv_dy = ddy(input.uv);
    const float3 position_dy_perp = cross(position_dy, normal);
    const float3 position_dx_perp = cross(normal, position_dx);
    const float3 tangent =
        position_dy_perp * uv_dx.x + position_dx_perp * uv_dy.x;
    const float3 bitangent =
        position_dy_perp * uv_dx.y + position_dx_perp * uv_dy.y;
    const float inverse_length = rsqrt(max(
        max(dot(tangent, tangent), dot(bitangent, bitangent)),
        1.0e-12f));
    const float3 tangent_normal = scene_textures[
        material.texture_normal].Sample(material_sampler, input.uv).xyz *
        2.0f - 1.0f;

    return normalize(
        tangent * (tangent_normal.x * inverse_length) +
        bitangent * (tangent_normal.y * inverse_length) +
        normal * tangent_normal.z);
}

float3 apply_fog(
    float3 color,
    float distance,
    float3 fog_color,
    float fog_start,
    float fog_end)
{
    float fog = saturate((distance - fog_start) / (fog_end - fog_start));
    return lerp(color, fog_color, fog);
}

float4 main(ForwardPixelInput input) : SV_TARGET
{
    const Material material = materials[material_id];

    float3 albedo = material.base_color;
#if FJR_ALPHA_TEST
    float opacity = 1.0f;
#endif

    if (material.texture_basecolor != INVALID_INDEX)
    {
        const float4 sample = scene_textures[
            material.texture_basecolor].Sample(
                material_sampler,
                input.uv);
        albedo *= sample.rgb;
#if FJR_ALPHA_TEST
        opacity *= sample.a;
#endif
    }

#if FJR_ALPHA_TEST
    if (material.texture_opacity != INVALID_INDEX)
    {
        opacity *= scene_textures[
            material.texture_opacity].Sample(
                material_sampler,
                input.uv).r;
    }

    clip(opacity - 0.5f);
#endif

    float roughness = material.roughness;
    if (material.texture_roughness != INVALID_INDEX)
    {
        roughness = scene_textures[
            material.texture_roughness].Sample(
                material_sampler,
                input.uv).r;
    }
    roughness = clamp(roughness, 0.045f, 1.0f);

    const float3 normal = normal_from_map(input, material);
    const float3 view_direction = normalize(
        cam_world_position - input.world_position);
    const float3 light_direction =
        normalize(float3(0.45f, 0.75f, -0.55f));
    const float3 half_vector = normalize(
        view_direction + light_direction);
    const float n_dot_l = saturate(dot(normal, light_direction));
    const float n_dot_v = saturate(dot(normal, view_direction));
    const float n_dot_h = saturate(dot(normal, half_vector));
    const float v_dot_h = saturate(dot(view_direction, half_vector));
    const float3 fresnel = fresnel_schlick(v_dot_h);
    const float3 diffuse = (1.0f - fresnel) * albedo / PI;
    const float3 specular = fresnel *
        distribution_ggx(n_dot_h, roughness) *
        visibility_smith_ggx(n_dot_v, n_dot_l, roughness) /
        max(4.0f * n_dot_v * n_dot_l, 1.0e-4f);

    float3 environment = environment_color * environment_intensity;
    if (environment_texture != INVALID_INDEX)
    {
        environment *= scene_textures[environment_texture].Sample(
            environment_sampler,
            environment_uv(normal)).rgb;
    }

    float3 final_color =
        (diffuse + specular) * n_dot_l + albedo * environment;
    float3 fog_color = float3(0.015f, 0.025f, 0.04f);
    float fog_start = 3000.0f;
    float fog_end = 3500.0f;
    float dist = length(input.world_position - cam_world_position);
    return float4(
        apply_fog(final_color, dist, fog_color, fog_start, fog_end),
        1.0f);
}
