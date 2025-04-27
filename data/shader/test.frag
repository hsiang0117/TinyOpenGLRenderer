#version 430 core
#include "data/shader/settings.glsl"

in VS_OUT{
	vec3 normal;
	vec2 texCoords;
	vec3 fragPos;
}fs_in;

out vec4 fragColor;

uniform sampler2D albedoMap;
uniform sampler2D ambientMap;
uniform sampler2D specularMap;
uniform sampler2D normalMap;
uniform sampler2D shininessMap;
#ifdef ENVIRONMENT_MAPPING
	uniform samplerCube skybox;
#endif

uniform vec3 cameraPos;

uniform int pointLightNum;
uniform int directionLightNum;
uniform int spotLightNum;

struct PointLight {
	vec4 position;
	vec4 color;
	float constant;
	float linear;
	float quadratic;
	float padding;
};

struct DirectionLight {
	vec4 direction;
	vec4 color;
};

struct SpotLight{
	vec4 position;
	vec4 direction;
	vec4 color;
	float cutOff;
	float outerCutOff;
	vec2 padding;
};

layout (std430, binding = 1) buffer PointLightBuffer{
	PointLight pointLights[];
};

layout (std430, binding = 2) buffer DirectionLightBuffer{
	DirectionLight directionLight;
};

layout (std430, binding = 3) buffer SpotLightBuffer{
	SpotLight spotLights[];
};

vec3 calculatePointLight(vec3 albedoColor, vec3 specularColor){
	vec3 result;
	for(int i=0;i<pointLightNum;i++){
		vec3 lightDir = normalize(pointLights[i].position.xyz - fs_in.fragPos);
		vec3 cameraDir = normalize(cameraPos - fs_in.fragPos);
		float diff = max(dot(lightDir,fs_in.normal),0);
		result += diff * albedoColor * pointLights[i].color.rgb;
		vec3 halfway = normalize(cameraDir + lightDir);
		float spec = pow(max(dot(halfway,fs_in.normal),0),32);
		result += spec * specularColor * pointLights[i].color.rgb;
	}
	return result;
}

vec3 calculateDirectionLight(vec3 albedoColor, vec3 specularColor){
	vec3 result;
	if(directionLightNum!=0){
		vec3 lightDir = normalize(-directionLight.direction.xyz);
		vec3 cameraDir = normalize(cameraPos - fs_in.fragPos);
		float diff = max(dot(lightDir,fs_in.normal),0);
		result += diff * albedoColor * directionLight.color.rgb;
		vec3 halfway = normalize(cameraDir + lightDir);
		float spec = pow(max(dot(halfway,fs_in.normal),0),32);
		result += spec * specularColor * directionLight.color.rgb;
	}
	return result;
};

vec3 calculateSpotLight(vec3 albedoColor, vec3 specularColor){
	vec3 result;
	for(int i=0;i < spotLightNum; i++){
		vec3 lightDir = normalize(spotLights[i].position.xyz - fs_in.fragPos);
		vec3 cameraDir = normalize(cameraPos - fs_in.fragPos);
		float theta = dot(lightDir,normalize(-spotLights[i].direction.xyz));
		float epsilon = spotLights[i].cutOff - spotLights[i].outerCutOff;
		float intensity = clamp((theta-spotLights[i].outerCutOff)/epsilon,0.0,1.0);
		float diff = max(dot(fs_in.normal,lightDir),0);
		result += diff * albedoColor * spotLights[i].color.rgb * intensity;
		vec3 halfway = normalize(cameraDir+lightDir);
		float spec = pow(max(dot(fs_in.normal,halfway),0.0),32);
		result += spec * specularColor * spotLights[i].color.rgb * intensity;
	}
	return result;
};

#ifdef ENVIRONMENT_MAPPING
vec3 calculateEnvironmentMapping(){
	vec3 result;
	vec3 cameraDir = normalize(cameraPos - fs_in.fragPos);
	vec3 reflectionDir = reflect(cameraDir,fs_in.normal);
	result = texture(skybox, reflectionDir).rgb*texture(ambientMap,fs_in.texCoords).rgb;
	return result;
}	
#endif

void main()
{
	vec3 albedoColor = texture(albedoMap, fs_in.texCoords).rgb;
	vec3 specularColor = texture(specularMap, fs_in.texCoords).rgb;
	vec3 result = 0.1 * albedoColor;
	result += calculateDirectionLight(albedoColor,specularColor);
	result += calculatePointLight(albedoColor,specularColor);
	result += calculateSpotLight(albedoColor,specularColor);
#ifdef ENVIRONMENT_MAPPING
	result += calculateEnvironmentMapping();
#endif
	fragColor = vec4(result,1.0);
}