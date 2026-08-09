#pragma once

cbuffer CameraConstants : register(b0)
{
	row_major float4x4 cam_view_projection;
	float3 cam_world_position;
	float cam_world_position_padding;
	float4 cam_normalized_frustum_planes[6];
    
	float lod_projection_scale;
	float lod_pixel_threshold;
	uint spatial_cluster_count;
	uint mesh_lod_count;

	float3 environment_color;
	float environment_intensity;
	uint environment_texture;
	float3 environment_padding;
};
