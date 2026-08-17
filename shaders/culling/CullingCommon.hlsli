#pragma once

#include "../common/ConstBufCamera.hlsli"
#include "../common/SphereCulling.hlsli"

StructuredBuffer<Mesh> meshes : register(t2);
StructuredBuffer<MeshLod> mesh_lods : register(t3);

void GetWorldSphere(
    Mesh mesh,
    InstanceTransform instance,
    out float3 center,
    out float radius)
{
    TransformSphere(
        float4(mesh.bounds_center, mesh.bounds_radius),
        instance,
        center,
        radius);
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

    const float projection_over_distance =
        lod_projection_scale / distance_to_camera;

    [loop]
    for (uint i = 0; i + 1 < mesh.lod_count; ++i)
    {
        const MeshLod lod =
            mesh_lods[mesh.lod_offset + i];

        const float screen_error_px =
            lod.next_lod_error *
            lod_error_scale *
            projection_over_distance;

        if (screen_error_px > lod_error_threshold_px)
            break;

        selected_lod = i + 1;
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
