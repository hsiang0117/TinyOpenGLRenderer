#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 boneIds;
layout (location = 6) in vec4 weights;

const int MAX_BONES = 500;
const int MAX_BONE_INFLUENCE = 4;

layout (binding = 8) uniform sampler2D boneMatrixTexture;

out VS_OUT{
	vec3 normal;
	vec2 texCoords;
	vec3 fragPos;
	vec4 fragPosLightSpace;
	mat3 TBN;
}vs_out;

uniform mat4 model;
layout (std140, binding=0) uniform Matrices
{
	mat4 view;
	mat4 projection;
};
uniform mat4 lightSpaceMatrix;

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
    vec3 totalNormal = vec3(0.0);
    vec3 totalTangent = vec3(0.0);
    vec3 totalBitangent = vec3(0.0);
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
            totalNormal = aNormal;
            totalTangent = aTangent;
            totalBitangent = aBitangent;
			break;
		}
		mat4 boneMatrix = getBoneMatrix(boneIds[i]);
        mat3 boneMatrix3 = mat3(boneMatrix);
        totalPosition += boneMatrix * vec4(aPos, 1.0) * weights[i];
        totalNormal   += boneMatrix3 * aNormal * weights[i];
        totalTangent  += boneMatrix3 * aTangent * weights[i];
        totalBitangent+= boneMatrix3 * aBitangent * weights[i];
		hasBone = true;
	}
	if(!hasBone)
	{
		totalPosition = vec4(aPos, 1.0);
		totalNormal = aNormal;
		totalTangent = aTangent;
		totalBitangent = aBitangent;
	}
	gl_Position = projection * view * model * totalPosition;
	vs_out.normal = normalize(mat3(transpose(inverse(model)))*totalNormal);
	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = (model*totalPosition).xyz;
	vs_out.fragPosLightSpace = lightSpaceMatrix * vec4(vs_out.fragPos, 1.0);
	vec3 T = normalize(mat3(model) * totalTangent);
	vec3 B = normalize(mat3(model) * totalBitangent);
	vec3 N = normalize(mat3(model) * totalNormal);
	vs_out.TBN = mat3(T, B, N);
}