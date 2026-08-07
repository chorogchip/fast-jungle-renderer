struct PixelInput {
    float4 position : SV_POSITION;
};

PixelInput main(uint vertex_id : SV_VertexID) {
    PixelInput output;
    const float2 position = float2(
        (vertex_id << 1) & 2,
        vertex_id & 2);
    output.position = float4(
        position.x * 2.0 - 1.0,
        1.0 - position.y * 2.0,
        0.0,
        1.0);
    return output;
}
