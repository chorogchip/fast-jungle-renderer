#include "../common/ConstBufCamera.hlsli"
#include "../common/RenderData.hlsli"
#include "../common/Quaternion.hlsli"
#include "../common/Shading.hlsli"
#include "Barycentric.hlsli"
#include "VisibilityCommon.hlsli"

#define RASTER_CLASS_PYRAMID 0
#define RASTER_CLASS_TERRAIN 1
#define RASTER_CLASS_OPAQUE 2
#define RASTER_CLASS_RIVER 3
#define RASTER_CLASS_ALPHA 4

StructuredBuffer<InstanceTransform> instances : register(t0);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t1);
Buffer<float4> vertices_pos : register(t2);

StructuredBuffer<OpaqueShadingVertex> vertices_shading : register(t3);

StructuredBuffer<AlphaVisibilityVertex> vertices_alpha_visibility : register(t4);
StructuredBuffer<uint> vertices_alpha_normal : register(t5);

Buffer<uint> indices : register(t6);
StructuredBuffer<SubMesh> submeshes : register(t7);

Texture2D<uint2> vis_buffer : register(t8);
StructuredBuffer<Material> materials : register(t9);
ByteAddressBuffer software_keys : register(t10);
Texture2D<float> hardware_depth : register(t11);
StructuredBuffer<SoftwareBatch> software_batches : register(t12);
StructuredBuffer<uint> software_visible_instances : register(t13);
StructuredBuffer<RasterCluster> raster_clusters : register(t14);
StructuredBuffer<uint> raster_cluster_vertices : register(t15);
StructuredBuffer<uint> raster_cluster_triangles : register(t16);
Texture2D<float4> scene_textures[] : register(t0, space1);

SamplerState material_sampler : register(s0);
SamplerState terrain_sampler : register(s1);

RWTexture2D<float4> frame_buffer : register(u0);

static const uint SOFTWARE_TRIANGLE_BITS = 7;
static const uint SOFTWARE_LOCAL_WORK_BITS = 17;
static const uint SOFTWARE_LOCAL_WORK_MASK =
    (1u << SOFTWARE_LOCAL_WORK_BITS) - 1u;

float3 DecodeNormalR10G10B10(uint packed)
{
    const uint3 encoded = uint3(
        packed & 0x3ffu,
        (packed >> 10u) & 0x3ffu,
        (packed >> 20u) & 0x3ffu);
    return float3(encoded) * (2.0f / 1023.0f) - 1.0f;
}

float2 DecodeUVR16G16(uint packed)
{
    const uint2 encoded = uint2(
        packed & 0xffffu,
        packed >> 16u);
    return float2(encoded) * (1.0f / 65535.0f);
}

float4 DecodePositionR16G16B16A16(uint packed_xy, uint packed_zw)
{
    const uint4 encoded = uint4(
        packed_xy & 0xffffu,
        packed_xy >> 16u,
        packed_zw & 0xffffu,
        packed_zw >> 16u);
    return float4(encoded) * (1.0f / 65535.0f);
}

float4 SampleMaterialTextureGrad(
    uint texture_id,
    bool clamp_to_edge,
    float2 uv,
    float2 uv_dx,
    float2 uv_dy)
{
    if (clamp_to_edge)
    {
        return scene_textures[
            NonUniformResourceIndex(texture_id)].SampleGrad(
                terrain_sampler, uv, uv_dx, uv_dy);
    }

    return scene_textures[
        NonUniformResourceIndex(texture_id)].SampleGrad(
            material_sampler, uv, uv_dx, uv_dy);
}

float3 NormalFromMap(
    float3 world_normal,
    float3 position_dx,
    float3 position_dy,
    float2 uv,
    float2 uv_dx,
    float2 uv_dy,
    Material material,
    bool clamp_to_edge)
{
    const float3 normal = normalize(world_normal);

    const float3 position_dy_perp = cross(position_dy, normal);
    const float3 position_dx_perp = cross(normal, position_dx);
    const float3 tangent = position_dy_perp * uv_dx.x + position_dx_perp * uv_dy.x;
    const float3 bitangent = position_dy_perp * uv_dx.y + position_dx_perp * uv_dy.y;
    const float inverse_length = rsqrt(max(
        max(dot(tangent, tangent), dot(bitangent, bitangent)),
        1.0e-12f));

    const float3 normal_sample = SampleMaterialTextureGrad(
        material.texture_normal,
        clamp_to_edge,
        uv,
        uv_dx,
        uv_dy).xyz;

    float3 tangent_normal;
    if ((material.flags & MATERIAL_FLAG_IMPOSTOR) != 0u)
    {
        // Impostors store the complete baked surface normal, including its
        // signed card-space Z component.
        tangent_normal = normal_sample * 2.0f - 1.0f;
    }
    else
    {
        tangent_normal.xy = normal_sample.xy * 2.0f - 1.0f;
        tangent_normal.z = sqrt(saturate(
            1.0f - dot(tangent_normal.xy, tangent_normal.xy)));
    }

    return normalize(
        tangent * (tangent_normal.x * inverse_length) +
        bitangent * (tangent_normal.y * inverse_length) +
        normal * tangent_normal.z);
}

[numthreads(16, 16, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    const uint2 pixel = tid.xy;
    if (pixel.x >= cam_pixel_width || pixel.y >= cam_pixel_height)
        return;
    
    const uint2 visibility = vis_buffer.Load(int3(pixel, 0));
    const uint key_offset =
        (pixel.y * cam_pixel_width + pixel.x) * 8u;
    const uint64_t software_key =
        software_keys.Load<uint64_t>(key_offset);
    bool software_winner = false;
    if (software_key != uint64_t(-1))
    {
        if (IsVisibilityEmpty(visibility))
        {
            software_winner = true;
        }
        else
        {
            const uint software_depth = uint(software_key >> 32u);
            const uint hardware_depth_bits = asuint(
                hardware_depth.Load(int3(pixel, 0)));
            software_winner = software_depth < hardware_depth_bits;
        }
    }
    if (!software_winner && IsVisibilityEmpty(visibility))
        return;

    uint submesh_id;
    uint instance_id;
    bool back_face;
    uint index0;
    uint index1;
    uint index2;
    if (software_winner)
    {
        const uint primitive_id = uint(software_key);
        const uint batch_id = primitive_id >>
            (SOFTWARE_LOCAL_WORK_BITS + SOFTWARE_TRIANGLE_BITS);
        const uint local_work =
            (primitive_id >> SOFTWARE_TRIANGLE_BITS) &
            SOFTWARE_LOCAL_WORK_MASK;
        const uint local_triangle = primitive_id &
            ((1u << SOFTWARE_TRIANGLE_BITS) - 1u);
        const SoftwareBatch batch = software_batches[batch_id];
        const uint local_instance = local_work / batch.cluster_count;
        const uint local_cluster = local_work % batch.cluster_count;
        instance_id = software_visible_instances[
            batch.visible_instance_offset + local_instance];
        submesh_id = batch.submesh_id;
        const RasterCluster cluster = raster_clusters[
            batch.cluster_offset + local_cluster];
        const uint packed_triangle = raster_cluster_triangles[
            cluster.triangle_offset + local_triangle];
        index0 = raster_cluster_vertices[
            cluster.vertex_offset + (packed_triangle & 0x7fu)];
        index1 = raster_cluster_vertices[
            cluster.vertex_offset + ((packed_triangle >> 7u) & 0x7fu)];
        index2 = raster_cluster_vertices[
            cluster.vertex_offset + ((packed_triangle >> 14u) & 0x7fu)];
        back_face = false;
    }
    else
    {
        const VisibilityRecord visibility_record =
            UnpackVisibility(visibility);
        submesh_id = visibility_record.submesh_id;
        instance_id = visibility_record.instance_id;
        back_face = visibility_record.back_face;
        const SubMesh hardware_submesh = submeshes[submesh_id];
        const uint index_offset = hardware_submesh.index_offset +
            visibility_record.triangle_id * 3u;
        index0 = indices[index_offset];
        index1 = indices[index_offset + 1u];
        index2 = indices[index_offset + 2u];
    }
    
    const uint raster_class = submeshes[submesh_id].raster_class;
    if (raster_class > RASTER_CLASS_ALPHA)
        return;
    
    const uint material_id = submeshes[submesh_id].material_id;
    
    const SubMesh submesh = submeshes[submesh_id];
    const Material material = materials[material_id];
    
    const uint vertex0 = submesh.base_vertex + index0;
    const uint vertex1 = submesh.base_vertex + index1;
    const uint vertex2 = submesh.base_vertex + index2;
    
    float3 p0;
    float3 p1;
    float3 p2;
    uint packed_uv0;
    uint packed_uv1;
    uint packed_uv2;
    uint packed_normal0;
    uint packed_normal1;
    uint packed_normal2;

    if (raster_class == RASTER_CLASS_ALPHA)
    {
        const AlphaVisibilityVertex vertex_data0 =
            vertices_alpha_visibility[vertex0];
        const AlphaVisibilityVertex vertex_data1 =
            vertices_alpha_visibility[vertex1];
        const AlphaVisibilityVertex vertex_data2 =
            vertices_alpha_visibility[vertex2];

        p0 = DecodePositionR16G16B16A16(
            vertex_data0.position_xy, vertex_data0.position_zw).xyz;
        p1 = DecodePositionR16G16B16A16(
            vertex_data1.position_xy, vertex_data1.position_zw).xyz;
        p2 = DecodePositionR16G16B16A16(
            vertex_data2.position_xy, vertex_data2.position_zw).xyz;
        packed_uv0 = vertex_data0.uv;
        packed_uv1 = vertex_data1.uv;
        packed_uv2 = vertex_data2.uv;
        packed_normal0 = vertices_alpha_normal[vertex0];
        packed_normal1 = vertices_alpha_normal[vertex1];
        packed_normal2 = vertices_alpha_normal[vertex2];
    }
    else
    {
        p0 = vertices_pos[vertex0].xyz;
        p1 = vertices_pos[vertex1].xyz;
        p2 = vertices_pos[vertex2].xyz;

        const OpaqueShadingVertex vertex_data0 = vertices_shading[vertex0];
        const OpaqueShadingVertex vertex_data1 = vertices_shading[vertex1];
        const OpaqueShadingVertex vertex_data2 = vertices_shading[vertex2];
        packed_uv0 = vertex_data0.uv;
        packed_uv1 = vertex_data1.uv;
        packed_uv2 = vertex_data2.uv;
        packed_normal0 = vertex_data0.normal;
        packed_normal1 = vertex_data1.normal;
        packed_normal2 = vertex_data2.normal;
    }

    const InstanceTransform instance = instances[instance_id];
    const VertexDecodeParams decode = vertex_decode_params[submesh_id];

    const float2 uv0 = decode.uv_min_extent.xy +
        DecodeUVR16G16(packed_uv0) * decode.uv_min_extent.zw;
    const float2 uv1 = decode.uv_min_extent.xy +
        DecodeUVR16G16(packed_uv1) * decode.uv_min_extent.zw;
    const float2 uv2 = decode.uv_min_extent.xy +
        DecodeUVR16G16(packed_uv2) * decode.uv_min_extent.zw;

    float3 op0 = decode.position_min.xyz + p0 * decode.position_extent.xyz;
    float3 op1 = decode.position_min.xyz + p1 * decode.position_extent.xyz;
    float3 op2 = decode.position_min.xyz + p2 * decode.position_extent.xyz;

    float3 norm0 = DecodeNormalR10G10B10(packed_normal0);
    float3 norm1 = DecodeNormalR10G10B10(packed_normal1);
    float3 norm2 = DecodeNormalR10G10B10(packed_normal2);

    if ((material.flags & MATERIAL_FLAG_IMPOSTOR) != 0u)
    {
        const float3 local_center_scaled =
            material.impostor_center * instance.scale;
        const float3 world_center = instance.position + RotateForwardVector(
            local_center_scaled,
            instance.rotation);

        float3 local_to_camera = RotateInverseVector(
            cam_world_position - world_center,
            instance.rotation);
        local_to_camera.y = 0.0f;
        local_to_camera = normalize(local_to_camera);

        const float3 local_forward = -local_to_camera;
        const float3 local_right = normalize(cross(
            float3(0.0f, 1.0f, 0.0f),
            local_forward));
        const float2 plane0 = float2(
            uv0.x * 2.0f - 1.0f,
            1.0f - uv0.y * 2.0f);
        const float2 plane1 = float2(
            uv1.x * 2.0f - 1.0f,
            1.0f - uv1.y * 2.0f);
        const float2 plane2 = float2(
            uv2.x * 2.0f - 1.0f,
            1.0f - uv2.y * 2.0f);

        op0 = material.impostor_center +
            local_right * (plane0.x * material.impostor_half_width) +
            float3(
                0.0f,
                plane0.y * material.impostor_half_height,
                0.0f);
        op1 = material.impostor_center +
            local_right * (plane1.x * material.impostor_half_width) +
            float3(
                0.0f,
                plane1.y * material.impostor_half_height,
                0.0f);
        op2 = material.impostor_center +
            local_right * (plane2.x * material.impostor_half_width) +
            float3(
                0.0f,
                plane2.y * material.impostor_half_height,
                0.0f);
        norm0 = local_to_camera;
        norm1 = local_to_camera;
        norm2 = local_to_camera;
    }

    const float3 wp0 = instance.position + RotateForwardVector(
        op0 * instance.scale, instance.rotation);
    const float3 wp1 = instance.position + RotateForwardVector(
        op1 * instance.scale, instance.rotation);
    const float3 wp2 = instance.position + RotateForwardVector(
        op2 * instance.scale, instance.rotation);

    const float4 cp0 = mul(float4(wp0, 1.0f), cam_view_projection);
    const float4 cp1 = mul(float4(wp1, 1.0f), cam_view_projection);
    const float4 cp2 = mul(float4(wp2, 1.0f), cam_view_projection);
    
    const float3 inv_scale = sign(instance.scale) / max(abs(instance.scale), 1.0e-8f);
    
    const float3 wnormal0 = normalize(RotateForwardVector(norm0 * inv_scale, instance.rotation));
    const float3 wnormal1 = normalize(RotateForwardVector(norm1 * inv_scale, instance.rotation));
    const float3 wnormal2 = normalize(RotateForwardVector(norm2 * inv_scale, instance.rotation));
    
    const float2 viewport = float2(cam_pixel_width, cam_pixel_height);

    const float2 pixel0 = ClipToPixel(cp0, viewport);
    const float2 pixel1 = ClipToPixel(cp1, viewport);
    const float2 pixel2 = ClipToPixel(cp2, viewport);
    if (software_winner)
    {
        const float2 edge1 = pixel1 - pixel0;
        const float2 edge2 = pixel2 - pixel0;
        back_face = edge1.x * edge2.y - edge1.y * edge2.x < 0.0f;
    }
    
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
    
    float3 normal_sample = normalize(
        InterpolateFloat3(wnormal0, wnormal1, wnormal2, perspective.value));
    if ((raster_class == RASTER_CLASS_ALPHA ||
        raster_class == RASTER_CLASS_RIVER) && back_face)
        normal_sample = -normal_sample;
    
    const uint material_mode = material.flags & MATERIAL_MODE_MASK;
    const bool clamp_material = raster_class == RASTER_CLASS_TERRAIN;
    const bool is_water = material_mode == MATERIAL_MODE_WATER;
    float3 albedo;
    float roughness;
    float3 normal;

    if (material_mode == MATERIAL_MODE_TEXTURED_PBR)
    {
        albedo = material.base_color * SampleMaterialTextureGrad(
            material.texture_basecolor,
            clamp_material,
            texCoord,
            texCoordDx,
            texCoordDy).rgb;

        roughness = SampleMaterialTextureGrad(
            material.texture_roughness,
            clamp_material,
            texCoord,
            texCoordDx,
            texCoordDy).r;

        normal = NormalFromMap(
            normal_sample, position_world_dx, position_world_dy,
            texCoord, texCoordDx, texCoordDy, material, clamp_material);
    }
    else if (material_mode == MATERIAL_MODE_CONSTANT_PBR)
    {
        albedo = material.base_color;
        roughness = material.roughness;
        normal = normal_sample;
    }
    else if (material_mode == MATERIAL_MODE_WATER)
    {
        albedo = material.base_color * SampleMaterialTextureGrad(
            material.texture_basecolor,
            false,
            texCoord,
            texCoordDx,
            texCoordDy).rgb;
        roughness = material.roughness;
        normal = normal_sample;
    }
    else
    {
        frame_buffer[pixel] = float4(1.0f, 0.0f, 1.0f, 1.0f);
        return;
    }
    roughness = clamp(roughness, 0.08f, 1.0f);
    
    const float3 view_vector = cam_world_position - position_world;
    const float dist = length(view_vector);
    const float3 view_direction = normalize(view_vector);
    const float3 sun_direction = normalize(float3(0.45f, 0.75f, -0.55f));
    const float3 half_vector = normalize(view_direction + sun_direction);
    
    const float nl = saturate(dot(normal, sun_direction));
    const float nv = saturate(dot(normal, view_direction));
    const float nh = saturate(dot(normal, half_vector));
    const float vh = saturate(dot(view_direction, half_vector));
    
    const float3 fresnel = FresnelSchlick(vh);
    const float3 diffuse = (1.0f - fresnel) * albedo / PI;
    
    const float3 specular =
        fresnel * NormDistGGX(nh, roughness) * GeometryGGX(nv, nl, roughness)
            / max(4.0f * nv * nl, 1.0e-4f);
    
    const float environment_intensity = 0.08f;
    const float sun_intensity = 2.5f;
    const float3 sun_color = float3(1.0f, 0.97f, 0.92f);
    // The source river base-color texture is intentionally black. Until the
    // full reflection path exists, retain a small water body color and a
    // view-dependent sky reflection instead of treating it as black diffuse.
    const float3 indirect_color = is_water
        ? float3(0.008f, 0.025f, 0.035f) +
            FresnelSchlick(nv) * float3(0.08f, 0.14f, 0.18f)
        : albedo * environment_intensity;
    const float3 pbr_color =
        (diffuse + specular) * nl * sun_color * sun_intensity
            + indirect_color;
    
    
    const float3 fog_color = float3(0.015f, 0.025f, 0.04f);
    const float fog_start = 3000.0f;
    const float fog_end = 3500.0f;
    
    const float3 final_color_linear =
        ApplyFog(pbr_color, dist, fog_color, fog_start, fog_end);
    const float4 final_color = float4(
        LinearToSRGB(final_color_linear), 1.0f);
    
    frame_buffer[pixel] = final_color;
}
