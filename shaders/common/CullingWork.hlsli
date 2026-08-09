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

    // Temporary impostor upper-bound probe: exclude only the forest instances
    // that a final-L6 card would replace. No card is drawn in this mode.
    if (cluster.impostor_probe != 0 &&
        local_lod + 1 == mesh.lod_count)
    {
        mesh_lod_id = MESH_LOD_CULLED;
        return false;
    }

    mesh_lod_id = mesh.lod_offset + local_lod;
    return mesh_lod_id < mesh_lod_count;
}
