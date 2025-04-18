#version 430 core

layout (location = 0)in vec3 aPos;

uniform mat4 lightMatrices;
uniform mat4 model;

void main()
{
	gl_Position = lightMatrices * model * vec4(aPos, 1.0);
}