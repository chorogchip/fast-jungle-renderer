#pragma once


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
