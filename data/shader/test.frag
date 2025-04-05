#version 430 core

in VS_OUT{
	vec3 normal;
	vec2 texCoords;
	vec3 fragPos;
}fs_in;

out vec4 fragColor;

void main()
{
	fragColor = vec4(fs_in.normal,1.0f);
}