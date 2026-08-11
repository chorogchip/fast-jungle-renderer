#pragma once

#include "RenderData.hlsli"
#include "Math.hlsli"

float NormDistGGX(float nh, float roughness)
{
    const float a = roughness * roughness;
    const float a2 = a * a;
    
    const float denominator = nh * nh * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * denominator * denominator, 1.0e-6f);
}

float GeometryGGX(float n_dot_v, float n_dot_l, float roughness)
{
    const float k = (roughness + 1.0f) * (roughness + 1.0f) * 0.125f;
    const float view = n_dot_v / max(n_dot_v * (1.0f - k) + k, 1.0e-4f);
    const float light = n_dot_l / max(n_dot_l * (1.0f - k) + k, 1.0e-4f);
    return view * light;
}

float3 FresnelSchlick(float vh)
{
    const float F0 = 0.04f;
    return F0.xxx + (1.0f - F0).xxx * pow(1.0f - vh, 5.0f);
}

float2 EnvironmentUV(float3 direction)
{
    direction = normalize(direction);
    return float2(
        atan2(direction.x, direction.z) / (2.0f * PI) + 0.5f,
        acos(clamp(direction.y, -1.0f, 1.0f)) / PI);
}

float3 ApplyFog(float3 color, float distance,
    float3 fog_color, float fog_start, float fog_end)
{
    float fog = saturate((distance - fog_start) / (fog_end - fog_start));
    return lerp(color, fog_color, fog);
}
