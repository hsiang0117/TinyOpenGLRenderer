#version 450 core
layout(location = 0) in vec2 aPos;
out VS_OUT {
    vec3 normal;
    vec2 texCoords;
    vec3 fragPos;
    vec4 fragPosLightSpace;
    mat3 TBN;
} vs_out;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vs_out.normal = vec3(0.0, 0.0, 1.0);
    vs_out.texCoords = vec2(0.5);
    vs_out.fragPos = vec3(0.0);
    vs_out.fragPosLightSpace = vec4(0.0, 0.0, 0.0, 1.0);
    vs_out.TBN = mat3(1.0);
}
