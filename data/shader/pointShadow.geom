#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 18) out;

uniform mat4 lightMatrices[6];
uniform int lightIndex;

out vec4 fragPos;

void main()
{
	for(int i = 0; i < 6; i++)
	{
		gl_Layer = lightIndex * 6 + i;
		for(int j = 0; j < 3; j++)
		{
			fragPos = gl_in[j].gl_Position;
			gl_Position = lightMatrices[i] * fragPos;
			EmitVertex();
		}
		EndPrimitive();
	}
}