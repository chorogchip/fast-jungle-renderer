#pragma once

#include "CullingCommon.hlsli"
#include "CullingResult.hlsli"

StructuredBuffer<SpatialCluster> spatial_clusters : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);

bool ResolveVisibleMeshLod(
    SpatialCluster cluster,
    Mesh mesh,
    uint instance_id,
    out uint mesh_lod_id)
{
    const InstanceTransform instance = instances[instance_id];

    float3 world_center;
    float world_radius;

    GetWorldSphere(
        mesh,
        instance,
        world_center,
        world_radius);

    if (!SphereInFrustum(
        cam_normalized_frustum_planes,
        world_center,
        world_radius))
    {
        mesh_lod_id = MESH_LOD_CULLED;
        return false;
    }

    const float distance_to_camera =
        ComputeDistanceToCamera(world_center);

    const uint local_lod =
        SelectConventionalMeshLod(
            mesh,
            world_radius,
            distance_to_camera);

    if (local_lod + 1 == mesh.lod_count)
    {
        const float projected_radius_px =
            ComputeProjectedRadiusPx(
                world_radius,
                distance_to_camera);

        if (ShouldCull(projected_radius_px))
        {
            mesh_lod_id = MESH_LOD_CULLED;
            return false;
        }

        if (mesh.impostor_direction_count > 0 &&
            ShouldUseImpostor(mesh, projected_radius_px))
        {
            const float4 inverse_rotation = float4(
                -instance.rotation.xyz,
                 instance.rotation.w);

            float3 camera_to_object =
                RotateForwardVector(
                    world_center - cam_world_position,
                    inverse_rotation);

            camera_to_object.y = 0.0f;

            if (dot(camera_to_object.xz, camera_to_object.xz) > 1.0e-10f)
            {
                float angle = atan2(
                    camera_to_object.x,
                    camera_to_object.z);

                if (angle < 0.0f)
                    angle += 6.28318530718f;

                uint direction = uint(floor(
                    angle *
                    (float(mesh.impostor_direction_count) /
                     6.28318530718f)
                    + 0.5f));

                if (direction == mesh.impostor_direction_count)
                    direction = 0;

                const uint lod_id =
                    mesh.impostor_card_lod_offset + direction;

                if (lod_id < mesh_lod_count)
                {
                    mesh_lod_id = lod_id;
                    return true;
                }

                mesh_lod_id = MESH_LOD_CULLED;
                return false;
            }
        }
    }

    const uint lod_id =
        mesh.lod_offset + local_lod;

    if (lod_id >= mesh_lod_count)
    {
        mesh_lod_id = MESH_LOD_CULLED;
        return false;
    }

    mesh_lod_id = lod_id;
    return true;
}
