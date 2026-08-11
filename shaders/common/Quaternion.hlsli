#pragma once

float3 RotateForwardVector(float3 value, float4 quaternion)
{
    const float3 twice_cross = 2.0f * cross(quaternion.xyz, value);
    return value +
        quaternion.w * twice_cross +
        cross(quaternion.xyz, twice_cross);
}

float3 RotateInverseVector(float3 value, float4 quaternion)
{
    return RotateForwardVector(
        value,
        float4(-quaternion.xyz, quaternion.w));
}
