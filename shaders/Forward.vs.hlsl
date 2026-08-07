
#include "common/CameraConstants.hlsli"

StructuredBuffer<SpatialCluster> spatial_clusters : register(t0);
StructuredBuffer<InstanceTransform> instances : register(t1);
StructuredBuffer<Mesh> meshes : register(t2);
StructuredBuffer<MeshLod> mesh_lods : register(t3);
StructuredBuffer<SubMesh> submeshes : register(t4);

RWStructuredBuffer<IndirectGPUDraw> indirect_draws : register(u0);
RWStructuredBuffer<uint> indirect_draw_counts : register(u1);
RWStructuredBuffer<uint> visible_instances : register(u2);

RWStructuredBuffer<uint> bin_counts : register(u3);
RWStructuredBuffer<uint> bin_offsets : register(u4);
RWStructuredBuffer<uint> bin_cursors : register(u5);

struct VS_In
{
    float3 position : POSITION;
};

struct VS_Out
{
    
};

void main(uint instance_id : SV_InstanceID)
{
    
}