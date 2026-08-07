#pragma once

#include "RenderData.hlsli"
#include "common/CameraConstants.hlsli"

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

uint SelectMeshLod(
    uint mesh_id,
    float3 world_center)
{
    Mesh mesh = meshes[mesh_id];

    if (mesh.lod_count <= 1)
        return 0;

    float distance_to_camera =
        length(world_center - cam_world_position);
    
    distance_to_camera = max(distance_to_camera, 1e-4f);

    uint selected_lod = 0;

    [loop]
    for (uint i = 0; i + 1 < mesh.lod_count; ++i)
    {
        MeshLod lod = mesh_lods[mesh.lod_offset + i];

        float error = lod.next_lod_error;

        float screen_error = error / distance_to_camera;
        
        const float LOD_ERROR_THRESHOLD = 0.01f;

        if (screen_error <= LOD_ERROR_THRESHOLD)
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