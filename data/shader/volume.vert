#version 450 core

layout (location = 0) in vec3 aPos;

layout (std140, binding = 0) uniform Matrices{
    mat4 view;
    mat4 projection;
};

uniform mat4 model;

out vec3 WorldPos;

void main() {
    WorldPos = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
