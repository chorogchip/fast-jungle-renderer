cbuffer DrawConstants : register(b1) {
    float4 object_color;
};

float4 main() : SV_TARGET {
    return object_color;
}
