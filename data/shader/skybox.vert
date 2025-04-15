#version 430 core

layout (location = 0) in vec3 aPos;

out vec3 texCoords;

layout (std140, binding = 0) uniform Matrices{
	mat4 view;
	mat4 projection;
};

void main(){
	texCoords = aPos;
	mat4 view = mat4(mat3(view));
	vec4 pos = projection * view * vec4(aPos, 1.0);
	gl_Position = pos.xyww;
}