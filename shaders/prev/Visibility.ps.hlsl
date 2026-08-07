cbuffer VisibilityConstants : register(b3) {
    uint visibility_draw_id;
};

uint2 main(uint primitive_id : SV_PrimitiveID) : SV_TARGET {
    return uint2(visibility_draw_id, primitive_id);
}
