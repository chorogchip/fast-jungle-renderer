#pragma once

#include "../culling.hlsli"

StructuredBuffer<SpatialCluster> spatial_clusters : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);

bool ResolveVisibleMeshLod(
    SpatialCluster cluster,
    uint instance_id,
    out uint mesh_lod_id)
{
    Mesh mesh = meshes[cluster.mesh_id];
    InstanceTransform instance = instances[instance_id];

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

    const uint local_lod = SelectMeshLod(
        cluster.mesh_id,
        world_center,
        world_radius);

    if (local_lod == MESH_LOD_CULLED)
    {
        mesh_lod_id = MESH_LOD_CULLED;
        return false;
    }

    if (local_lod + 1 == mesh.lod_count &&
        mesh.impostor_direction_count != 0)
    {
        const float4 inverse_rotation = float4(
            -instance.rotation.xyz,
            instance.rotation.w);
        float3 camera_to_object = RotateQuaternion(
            world_center - cam_world_position,
            inverse_rotation);
        camera_to_object.y = 0.0f;
        const float horizontal_length = length(camera_to_object);
        if (horizontal_length > 1.0e-5f)
        {
            const float angle = atan2(
                camera_to_object.x,
                camera_to_object.z);
            const float direction_angle =
                6.28318530717958647692f /
                float(mesh.impostor_direction_count);
            const int rounded_direction = int(floor(
                (angle + 0.5f * direction_angle) / direction_angle));
            const int direction_count = int(mesh.impostor_direction_count);
            const uint direction = uint(
                (rounded_direction % direction_count + direction_count) %
                direction_count);
            mesh_lod_id = mesh.impostor_card_lod_offset + direction;
            return mesh_lod_id < mesh_lod_count;
        }
    }

    mesh_lod_id = mesh.lod_offset + local_lod;
    return mesh_lod_id < mesh_lod_count;
}
