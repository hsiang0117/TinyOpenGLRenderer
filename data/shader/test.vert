#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

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


void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
	vs_out.normal = normalize(mat3(transpose(inverse(model)))*aNormal);
	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = (model*vec4(aPos,1.0)).xyz;
	vs_out.fragPosLightSpace = lightSpaceMatrix * vec4(vs_out.fragPos, 1.0);
	vec3 T = normalize(mat3(model) * aTangent);
	vec3 B = normalize(mat3(model) * aBitangent);
	vec3 N = normalize(mat3(model) * aNormal);
	vs_out.TBN = mat3(T, B, N);
}