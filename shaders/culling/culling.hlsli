#pragma once

#include "../common/ConstBufCamera.hlsli"
#include "../common/ConstantDraw.hlsli"

StructuredBuffer<Mesh> meshes : register(t2);
StructuredBuffer<MeshLod> mesh_lods : register(t3);

float3 RotateQuaternion(float3 v, float4 q)
{
    float3 t = 2.0f * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

void GetWorldSphere(
    Mesh mesh,
    InstanceTransform instance,
    out float3 center,
    out float radius)
{
    float3 local_center = mesh.bounds_center * instance.scale;

    center =
        instance.position +
        RotateQuaternion(local_center, instance.rotation);
    
    float3 abs_scale = abs(instance.scale);
    float max_scale = max(abs_scale.x, max(abs_scale.y, abs_scale.z));

    radius = mesh.bounds_radius * max_scale;
}

bool SphereInFrustum(
    float4 frustum_planes[6],
    float3 center,
    float radius)
{
    [unroll]
    for (uint i = 0; i < 6; ++i)
    {
        float4 plane = frustum_planes[i];

        if (dot(plane.xyz, center) + plane.w < -radius)
            return false;
    }

    return true;
}

float ComputeDistanceToCamera(float3 world_center)
{
    return max(
        length(world_center - cam_world_position),
        1.0e-4f);
}

float ComputeProjectedRadiusPx(
    float world_radius,
    float distance_to_camera)
{
    return
        world_radius *
        lod_projection_scale /
        max(distance_to_camera, 1.0e-4f);
}

uint SelectConventionalMeshLod(
    Mesh mesh,
    float world_radius,
    float distance_to_camera)
{
    uint selected_lod = 0;

    const float lod_error_scale =
        mesh.bounds_radius > 0.0f
        ? world_radius / mesh.bounds_radius
        : 1.0f;

    const float safe_distance =
        max(distance_to_camera, 1.0e-4f);

    [loop]
    for (uint i = 0; i + 1 < mesh.lod_count; ++i)
    {
        const MeshLod lod =
            mesh_lods[mesh.lod_offset + i];

        const float world_error =
            lod.next_lod_error * lod_error_scale;

        const float screen_error_px =
            world_error *
            lod_projection_scale /
            safe_distance;

        if (screen_error_px <= lod_error_threshold_px)
        {
            selected_lod = i + 1;
        }
        else
        {
            break;
        }
    }

    return selected_lod;
}

bool ShouldCull(float projected_radius_px)
{
    return projected_radius_px <= cull_radius_px;
}

bool ShouldUseImpostor(
    Mesh mesh,
    float projected_radius_px)
{
    return
        mesh.impostor_direction_count != 0 &&
        projected_radius_px <= impostor_transition_radius_px;
}