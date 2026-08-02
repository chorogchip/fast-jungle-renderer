struct VertexOutput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VertexOutput main(uint vertex_id : SV_VertexID) {
    const float2 positions[3] = {
        float2( 0.0,  0.65),
        float2( 0.65, -0.55),
        float2(-0.65, -0.55)
    };

    const float4 colors[3] = {
        float4(1.0, 0.2, 0.2, 1.0),
        float4(0.2, 1.0, 0.3, 1.0),
        float4(0.2, 0.5, 1.0, 1.0)
    };

    VertexOutput output;
    output.position = float4(positions[vertex_id], 0.0, 1.0);
    output.color = colors[vertex_id];
    return output;
}
