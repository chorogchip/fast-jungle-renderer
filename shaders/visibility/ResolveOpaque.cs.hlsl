#include "../common/ConstBufCamera.hlsli"
#include "../common/RenderData.hlsli"

#define RASTER_CLASS_PYRAMID 0
#define RASTER_CLASS_TERRAIN 1
#define RASTER_CLASS_RIVER 2
#define RASTER_CLASS_OPAQUE 3
#define RASTER_CLASS_ALPHA 4
#define RASTER_CLASS_BILBOARD 5

Texture2D<uint2> vis_buffer : register(t0);

Buffer<uint> indices : register(t1);
StructuredBuffer<InstanceTransform> instances : register(t2);
Buffer<float4> vertices_pos : register(t3);
Buffer<float4> vertices_uvnorm : register(t4);
StructuredBuffer<SubMesh> submeshes : register(t5);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t6);

StructuredBuffer<Material> materials : register(t7);
Texture2D<float4> scene_textures[] : register(t8);

SamplerState material_sampler : register(s0);
SamplerState environment_sampler : register(s1);

RWTexture2D<float4> frame_buffer : register(u0);

cbuffer ResolveConstants : register(b1)
{
    uint g_pixel_width;
    uint g_pixel_height;
};

static const float PI = 3.14159265358979323846f;


float3 RotateForwardVector(float3 value, float4 quaternion)
{
    const float3 twice_cross = 2.0f * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twice_cross +
        cross(quaternion.xyz, twice_cross);
}

float3 DecodeOctNormal(float2 encoded)
{
    float2 f = encoded * 2.0f - 1.0f;
    float3 normal = float3(f, 1.0f - abs(f.x) - abs(f.y));

    float t = saturate(-normal.z);
    normal.xy += float2(
        normal.x >= 0.0f ? -t : t,
        normal.y >= 0.0f ? -t : t);

    return normalize(normal);
}

float2 ClipToPixel(float4 clip_pos, float2 viewport_size)
{
    float2 ndc = clip_pos.xy / clip_pos.w;
    return float2(
        (ndc.x * 0.5f + 0.5f) * viewport_size.x,
        (0.5f - ndc.y * 0.5f) * viewport_size.y);
}

struct BarycentricGradient
{
    float3 value;
    float3 dx;
    float3 dy;
};

BarycentricGradient CalcBaryWithGrad(
    float2 p, float2 p0, float2 p1, float2 p2)
{
    float2 e1 = p1 - p0;
    float2 e2 = p2 - p0;
    float2 d = p - p0;
    
    float det = e1.x * e2.y - e1.y * e2.x;
    float det_inv = rcp(det);
    
    float lambda1 = (d.x * e2.y - d.y * e2.x) * det_inv;
    float lambda2 = (d.y * e1.x - d.x * e1.y) * det_inv;
    float lambda0 = 1.0f - lambda1 - lambda2;
    
    float2 grad_lambda1 = float2(e2.y, -e2.x) * det_inv;
    float2 grad_lambda2 = float2(-e1.y, e1.x) * det_inv;
    float2 grad_lambda0 = -grad_lambda1 - grad_lambda2;
    
    BarycentricGradient result;
    result.value = float3(lambda0, lambda1, lambda2);
    result.dx = float3(grad_lambda0.x, grad_lambda1.x, grad_lambda2.x);
    result.dy = float3(grad_lambda0.y, grad_lambda1.y, grad_lambda2.y);

    return result;
}

struct PerspectiveBarycentricGradient
{
    float3 value;
    float3 dx;
    float3 dy;
};

PerspectiveBarycentricGradient CalcPerspectiveBaryWithGrad(
    BarycentricGradient bary, float3 inv_w)
{
    float D = dot(bary.value, inv_w);
    float inv_D = rcp(D);
    
    float Dx = dot(bary.dx, inv_w);
    float Dy = dot(bary.dy, inv_w);
    
    PerspectiveBarycentricGradient result;
    result.value = bary.value * inv_w * inv_D;
    result.dx = (bary.dx * inv_w - result.value * Dx) * inv_D;
    result.dy = (bary.dy * inv_w - result.value * Dy) * inv_D;

    return result;
}

float2 InterpolateFloat2(float2 v0, float2 v1, float2 v2, float3 bary)
{
    return v0 * bary.x + v1 * bary.y + v2 * bary.z;
}

float3 InterpolateFloat3(float3 v0, float3 v1, float3 v2, float3 bary)
{
    return v0 * bary.x + v1 * bary.y + v2 * bary.z;
}


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

float2 environment_uv(float3 direction)
{
    direction = normalize(direction);
    return float2(
        atan2(direction.x, direction.z) / (2.0f * PI) + 0.5f,
        acos(clamp(direction.y, -1.0f, 1.0f)) / PI);
}

float3 normal_from_map(
    float3 world_normal,
    float3 position_dx,
    float3 position_dy,
    float2 uv,
    float2 uv_dx,
    float2 uv_dy,
    Material material)
{
    const float3 normal = normalize(world_normal);

    const float3 position_dy_perp = cross(position_dy, normal);
    const float3 position_dx_perp = cross(normal, position_dx);
    const float3 tangent = position_dy_perp * uv_dx.x + position_dx_perp * uv_dy.x;
    const float3 bitangent = position_dy_perp * uv_dx.y + position_dx_perp * uv_dy.y;
    const float inverse_length = rsqrt(max(
        max(dot(tangent, tangent), dot(bitangent, bitangent)),
        1.0e-12f));

    const float3 normal_sample = scene_textures[
        NonUniformResourceIndex(material.texture_normal)].SampleGrad(
            material_sampler,
            uv,
            uv_dx,
            uv_dy).xyz;

    float3 tangent_normal;
    tangent_normal.xy = normal_sample.xy * 2.0f - 1.0f;
    tangent_normal.z = sqrt(saturate(
        1.0f - dot(tangent_normal.xy, tangent_normal.xy)));

    return normalize(
        tangent * (tangent_normal.x * inverse_length) +
        bitangent * (tangent_normal.y * inverse_length) +
        normal * tangent_normal.z);
}

float3 ApplyFog(float3 color, float distance,
    float3 fog_color, float fog_start, float fog_end)
{
    float fog = saturate((distance - fog_start) / (fog_end - fog_start));
    return lerp(color, fog_color, fog);
}


[numthreads(16, 16, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    const uint2 pixel = tid.xy;
    if (pixel.x >= g_pixel_width || pixel.y >= g_pixel_height)
        return;
    
    const uint2 visibility = vis_buffer.Load(int3(pixel, 0));
    if (visibility.y == 0xffffffff)
        return;
    
    const uint submesh_id =
        ((visibility.x >> 22) & 0x3ffu) | ((visibility.y & 1u) << 10);
    
    const uint raster_class = submeshes[submesh_id].raster_class;
    if (raster_class >= RASTER_CLASS_ALPHA)
        return;
    
    const uint material_id = submeshes[submesh_id].material_id;
    const uint triangle_id = visibility.x & 0x3fffffu;
    const uint instance_id = visibility.y >> 1;
    
    const SubMesh submesh = submeshes[submesh_id];
    
    const uint index_ofs = submesh.index_offset + triangle_id * 3;
    const uint index0 = indices[index_ofs];
    const uint index1 = indices[index_ofs + 1];
    const uint index2 = indices[index_ofs + 2];
    
    const uint vertex0 = submesh.base_vertex + index0;
    const uint vertex1 = submesh.base_vertex + index1;
    const uint vertex2 = submesh.base_vertex + index2;
    
    const float3 p0 = vertices_pos[vertex0].xyz;
    const float3 p1 = vertices_pos[vertex1].xyz;
    const float3 p2 = vertices_pos[vertex2].xyz;
    
    const float4 uvnorm0 = vertices_uvnorm[vertex0];
    const float4 uvnorm1 = vertices_uvnorm[vertex1];
    const float4 uvnorm2 = vertices_uvnorm[vertex2];

    const InstanceTransform instance = instances[instance_id];
    const VertexDecodeParams decode = vertex_decode_params[submesh_id];
    
    const float3 op0 = decode.position_min.xyz + p0 * decode.position_extent.xyz;
    const float3 op1 = decode.position_min.xyz + p1 * decode.position_extent.xyz;
    const float3 op2 = decode.position_min.xyz + p2 * decode.position_extent.xyz;
    
    const float3 wp0 = instance.position + RotateForwardVector(op0 * instance.scale, instance.rotation);
    const float3 wp1 = instance.position + RotateForwardVector(op1 * instance.scale, instance.rotation);
    const float3 wp2 = instance.position + RotateForwardVector(op2 * instance.scale, instance.rotation);
    
    const float4 cp0 = mul(float4(wp0, 1.0f), cam_view_projection);
    const float4 cp1 = mul(float4(wp1, 1.0f), cam_view_projection);
    const float4 cp2 = mul(float4(wp2, 1.0f), cam_view_projection);
    
    const float2 uv0 = decode.uv_min_extent.xy + uvnorm0.xy * decode.uv_min_extent.zw;
    const float2 uv1 = decode.uv_min_extent.xy + uvnorm1.xy * decode.uv_min_extent.zw;
    const float2 uv2 = decode.uv_min_extent.xy + uvnorm2.xy * decode.uv_min_extent.zw;
    
    const float3 norm0 = DecodeOctNormal(uvnorm0.zw);
    const float3 norm1 = DecodeOctNormal(uvnorm1.zw);
    const float3 norm2 = DecodeOctNormal(uvnorm2.zw);
    
    const float3 inv_scale = sign(instance.scale) / max(abs(instance.scale), 1.0e-8f);
    
    const float3 wnormal0 = normalize(RotateForwardVector(norm0 * inv_scale, instance.rotation));
    const float3 wnormal1 = normalize(RotateForwardVector(norm1 * inv_scale, instance.rotation));
    const float3 wnormal2 = normalize(RotateForwardVector(norm2 * inv_scale, instance.rotation));
    
    const float2 viewport = float2(g_pixel_width, g_pixel_height);

    const float2 pixel0 = ClipToPixel(cp0, viewport);
    const float2 pixel1 = ClipToPixel(cp1, viewport);
    const float2 pixel2 = ClipToPixel(cp2, viewport);
    
    const float2 sample_position = float2(pixel) + float2(0.5f, 0.5f);
    
    const BarycentricGradient bary = CalcBaryWithGrad(sample_position, pixel0, pixel1, pixel2);
    const float3 inv_w = rcp(float3(cp0.w, cp1.w, cp2.w));
    const PerspectiveBarycentricGradient perspective = CalcPerspectiveBaryWithGrad(bary, inv_w);
    
    const float2 texCoord = InterpolateFloat2(uv0, uv1, uv2, perspective.value);
    const float2 texCoordDx = InterpolateFloat2(uv0, uv1, uv2, perspective.dx);
    const float2 texCoordDy = InterpolateFloat2(uv0, uv1, uv2, perspective.dy);
    
    const float3 position_world = InterpolateFloat3(wp0, wp1, wp2, perspective.value);
    const float3 position_world_dx = InterpolateFloat3(wp0, wp1, wp2, perspective.dx);
    const float3 position_world_dy = InterpolateFloat3(wp0, wp1, wp2, perspective.dy);
    
    const float3 normal_sample = normalize(InterpolateFloat3(wnormal0, wnormal1, wnormal2, perspective.value));
    
    const Material material = materials[material_id];

    float3 albedo = material.base_color *
        scene_textures[
            NonUniformResourceIndex(material.texture_basecolor)].SampleGrad(
                material_sampler, texCoord, texCoordDx, texCoordDy).rgb;

    float roughness = material.roughness *
        scene_textures[
            NonUniformResourceIndex(material.texture_roughness)].SampleGrad(
                material_sampler, texCoord, texCoordDx, texCoordDy).r;
    
    roughness = clamp(roughness, 0.045f, 1.0f);
    
    const float3 normal = normal_from_map(
        normal_sample, position_world_dx, position_world_dy,
        texCoord, texCoordDx, texCoordDy, material);
    
    const float3 view_vector = cam_world_position - position_world;
    const float dist = length(view_vector);
    const float3 view_direction = normalize(view_vector);
    const float3 light_direction = normalize(float3(0.45f, 0.75f, -0.55f));
    const float3 half_vector = normalize(view_direction + light_direction);
    
    const float nl = saturate(dot(normal, light_direction));
    const float nv = saturate(dot(normal, view_direction));
    const float nh = saturate(dot(normal, half_vector));
    const float vh = saturate(dot(view_direction, half_vector));
    
    const float3 fresnel = FresnelSchlick(vh);
    const float3 diffuse = (1.0f - fresnel) * albedo / PI;
    
    const float3 specular =
        fresnel * NormDistGGX(nh, roughness) * GeometryGGX(nv, nl, roughness)
            / max(4.0f * nv * nl, 1.0e-4f);
    
    float3 environment = environment_color * environment_intensity *
        scene_textures[environment_texture].SampleLevel(
            environment_sampler,
            environment_uv(normal),
            0.0f).rgb;

    
    float3 pbr_color = (diffuse + specular) * nl + albedo * environment;
    
    
    float3 fog_color = float3(0.015f, 0.025f, 0.04f);
    float fog_start = 3000.0f;
    float fog_end = 3500.0f;
    
    float4 final_color = float4(
        ApplyFog(pbr_color, dist, fog_color, fog_start, fog_end),
        1.0f);
    
    frame_buffer[pixel] = final_color;
}

// ªÏ∑¡¡‡