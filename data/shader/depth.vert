#version 450 core

layout (location = 0)in vec3 aPos;
layout (location = 5) in ivec4 boneIds;
layout (location = 6) in vec4 weights;

const int MAX_BONES = 500;
const int MAX_BONE_INFLUENCE = 4;
layout (binding = 8) uniform sampler2D boneMatrixTexture;

uniform mat4 lightMatrices;
uniform mat4 model;

mat4 getBoneMatrix(int index)
{
	return mat4(
		texelFetch(boneMatrixTexture, ivec2(0, index), 0),
		texelFetch(boneMatrixTexture, ivec2(1, index), 0),
		texelFetch(boneMatrixTexture, ivec2(2, index), 0),
		texelFetch(boneMatrixTexture, ivec2(3, index), 0)
		);
}

void main()
{
	vec4 totalPosition = vec4(0.0);
	bool hasBone = false;
	for(int i = 0; i < MAX_BONE_INFLUENCE; i++)
	{
		if(boneIds[i] == -1)
			continue;
		if(weights[i] == -1.0)
			continue;
		if(boneIds[i] >= MAX_BONES)
		{
			totalPosition = vec4(aPos, 1.0);
			break;
		}
		mat4 boneMatrix = getBoneMatrix(boneIds[i]);
        mat3 boneMatrix3 = mat3(boneMatrix);
        totalPosition += boneMatrix * vec4(aPos, 1.0) * weights[i];
		hasBone = true;
	}
	if(!hasBone)
	{
		totalPosition = vec4(aPos, 1.0);
	}
	gl_Position = lightMatrices * model * totalPosition;
}