#pragma once

static const uint MESH_LOD_CULLED = 0xffffffffu;

struct InstanceTransform
{
    float3 position;
    float4 rotation;
    float3 scale;
};

struct Material
{
    float3 base_color;
    float roughness;
    
    uint texture_basecolor;
    uint texture_normal;
    uint texture_roughness;
    uint texture_opacity;

    uint flags;
    float3 impostor_center;
    float impostor_half_width;
    float impostor_half_height;
    float2 padding;
};

struct SubMesh
{
    uint material_id;
    uint raster_class;
    uint index_offset;
    uint index_count;
    uint base_vertex;
};

struct MeshLod
{
    uint submesh_offset;
    uint submesh_count;
    float lod_error;
    float next_lod_error;
};

struct Mesh
{
    float3 bounds_center;
    float bounds_radius;
    uint lod_offset;
    uint lod_count;
    uint impostor_card_lod_offset;
    uint impostor_direction_count;
};

struct SpatialCluster
{
    float3 bounds_center;
    float bounds_radius;
    uint mesh_id;
    uint instance_offset;
    uint instance_count;
    uint padding;
};

struct IndirectGPUDraw
{
    uint visible_instance_offset;
    uint material_id;
    uint submesh_id;
    
    uint index_count_per_instance;
    uint instance_count;
    uint start_index_location;
    uint base_vertex_location;
    uint start_instance_location;
    
    uint padding;
};

/*

cbuffer CameraConstants : register(b0)

StructuredBuffer<SpatialCluster> spatial_clusters : register(t0);
StructuredBuffer<InstanceTransform> instances     : register(t1);
StructuredBuffer<Mesh> meshes                     : register(t2);
StructuredBuffer<MeshLod> mesh_lods               : register(t3);
StructuredBuffer<SubMesh> submeshes               : register(t4);
StructuredBuffer<VertexDecodeParams> vertex_decode_params : register(t5);

RWStructuredBuffer<IndirectGPUDraw> indirect_draws : register(u0);
RWStructuredBuffer<uint> indirect_draw_counts      : register(u1);
RWStructuredBuffer<uint> visible_instances         : register(u2);

RWStructuredBuffer<uint> bin_counts  : register(u3);
RWStructuredBuffer<uint> bin_offsets : register(u4);
RWStructuredBuffer<uint> bin_cursors : register(u5);

*/
