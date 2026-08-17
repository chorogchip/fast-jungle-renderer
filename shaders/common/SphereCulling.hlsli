#pragma once

#include "Quaternion.hlsli"
#include "RenderData.hlsli"

void TransformSphere(
    float4 local_sphere,
    InstanceTransform instance,
    out float3 center,
    out float radius)
{
    center = instance.position + RotateForwardVector(
        local_sphere.xyz * instance.scale,
        instance.rotation);

    const float3 abs_scale = abs(instance.scale);
    radius = local_sphere.w * max(
        abs_scale.x,
        max(abs_scale.y, abs_scale.z));
}

bool SphereInFrustum(
    float4 frustum_planes[6],
    float3 center,
    float radius)
{
    [unroll]
    for (uint i = 0; i < 6; ++i)
    {
        const float4 plane = frustum_planes[i];
        if (dot(plane.xyz, center) + plane.w < -radius)
            return false;
    }

    return true;
}
