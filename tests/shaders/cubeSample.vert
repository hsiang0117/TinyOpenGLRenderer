#version 450 core
layout(location = 0) in vec2 aPos;
out vec3 texCoords;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    texCoords = vec3(0.0, 0.0, 1.0);
}
