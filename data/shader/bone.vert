#version 450 core

layout (location = 0) in vec3 aPos;

uniform mat4 model;
layout (std140, binding=0) uniform Matrices
{
	mat4 view;
	mat4 projection;
};

void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
	float minSize = 1.0;
	float maxSize = 5.0;
	float near = 0.1;
	float far = 100.0;
	float viewSpaceDepth = abs((view * model * vec4(aPos, 1.0)).z);
	float normDepth = clamp((viewSpaceDepth - near) / (far - near), 0.0, 1.0);
	gl_PointSize = mix(maxSize, minSize, normDepth);
}