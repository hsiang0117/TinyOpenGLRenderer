#version 450 core
#include "data/shader/settings.glsl"

in VS_OUT{
	vec3 normal;
	vec2 texCoords;
	vec3 fragPos;
	vec4 fragPosLightSpace;
	mat3 TBN;
}fs_in;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 brightColor;

uniform bool useDefaultMaterial;

layout (binding = 0) uniform sampler2D albedoMap;
layout (binding = 1) uniform sampler2D ambientMap;
layout (binding = 2) uniform sampler2D specularMap;
uniform bool hasNormalMap;
layout (binding = 3) uniform sampler2D normalMap;
layout (binding = 4) uniform sampler2D shininessMap;
#ifdef USE_ENVIRONMENT_MAPPING
	layout (binding = 5) uniform samplerCube skybox;
#endif
layout (binding = 6) uniform sampler2D shadowMap;
layout (binding = 7) uniform samplerCubeArray shadowMapArray;

uniform vec3 cameraPos;

uniform int pointLightNum;
uniform int directionLightNum;
uniform int spotLightNum;

struct PointLight {
	vec4 position;
	vec3 color;
	float brightness;
	float constant;
	float linear;
	float quadratic;
	float farPlane;
};

struct DirectionLight {
	vec4 direction;
	vec3 color;
	float brightness;
};

struct SpotLight{
	vec4 position;
	vec4 direction;
	vec3 color;
	float brightness;
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

vec3 calculatePointLight(vec3 albedoColor, vec3 specularColor, vec3 cameraDir, vec3 normal){
	vec3 result;
	for(int i=0;i<pointLightNum;i++){
		float attenuation = 1.0 / (pointLights[i].constant + pointLights[i].linear * length(pointLights[i].position.xyz - fs_in.fragPos) + pointLights[i].quadratic * length(pointLights[i].position.xyz - fs_in.fragPos) * length(pointLights[i].position.xyz - fs_in.fragPos));
		vec3 lightDir = normalize(pointLights[i].position.xyz - fs_in.fragPos);
		float diff = max(dot(lightDir,normal),0);
		vec3 halfway = normalize(cameraDir + lightDir);
		float spec = pow(max(dot(halfway,normal),0),32);
		result += (diff * albedoColor + spec * specularColor) * pointLights[i].color * pointLights[i].brightness * attenuation;
	}
	return result;
}

vec3 calculateDirectionLight(vec3 albedoColor, vec3 specularColor,vec3 cameraDir, vec3 normal){
	vec3 result;
	if(directionLightNum!=0){
		vec3 lightDir = normalize(-directionLight.direction.xyz);
		float diff = max(dot(lightDir,normal),0);
		vec3 halfway = normalize(cameraDir + lightDir);
		float spec = pow(max(dot(halfway,normal),0),32);
		result += (diff * albedoColor + spec * specularColor) * directionLight.color * directionLight.brightness;
	}
	return result;
};

vec3 calculateSpotLight(vec3 albedoColor, vec3 specularColor, vec3 cameraDir, vec3 normal){
	vec3 result;
	for(int i=0;i < spotLightNum; i++){
		vec3 lightDir = normalize(spotLights[i].position.xyz - fs_in.fragPos);
		float theta = dot(lightDir,normalize(-spotLights[i].direction.xyz));
		float epsilon = spotLights[i].cutOff - spotLights[i].outerCutOff;
		float intensity = clamp((theta-spotLights[i].outerCutOff)/epsilon,0.0,1.0);
		float diff = max(dot(lightDir,normal),0);
		vec3 halfway = normalize(cameraDir+lightDir);
		float spec = pow(max(dot(halfway,normal),0),32);
		result += (diff * albedoColor + spec * specularColor) * spotLights[i].color * spotLights[i].brightness * intensity;
	}
	return result;
};

#ifdef USE_ENVIRONMENT_MAPPING
vec3 calculateEnvironmentMapping(){
	vec3 result;
	vec3 cameraDir = normalize(cameraPos - fs_in.fragPos);
	vec3 reflectionDir = reflect(cameraDir,fs_in.normal);
	result = texture(skybox, reflectionDir).rgb*texture(ambientMap,fs_in.texCoords).rgb;
	return result;
}	
#endif

float calculateDirectionLightShadow(vec3 normal){
	float shadow = 0.0;
	if (directionLightNum==0){
		return shadow;
	}
	vec3 lightDir = normalize(-directionLight.direction.xyz);
	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	vec3 projCoords = fs_in.fragPosLightSpace.xyz / fs_in.fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;
	if (projCoords.z > 1.0){
		return 0.0;
	}
	float currentDepth = projCoords.z;
#ifdef PCF_SHADOW
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
	for(int x = -2; x <= 2; ++x)
	{
		for(int y = -2; y <= 2; ++y)
		{
			float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
			shadow += currentDepth - bias > pcfDepth ? 0.75 : 0.0;        
		}    
	}
	shadow /= 25.0;
#else
	float closestDepth = texture(shadowMap, projCoords.xy).r;
	shadow = currentDepth - bias > closestDepth ? 0.75 : 0.0;
#endif
	return shadow;
}

float calculatePointLightShadow(vec3 normal){
	float shadow = 0;
	for(int i = 0; i < pointLightNum; i++){
		vec3 fragToLight = fs_in.fragPos - pointLights[i].position.xyz;
		float currentDepth = length(fragToLight);
		float bias = max(0.5 * (1.0 - dot(normal, normalize(fragToLight))), 0.05);
		float farPlane = pointLights[i].farPlane;
		if(currentDepth>farPlane){
			continue;
		}
#ifdef PCF_SHADOW
		float offset = 0.1;
		float samples = 4.0;
		float tempShadow = 0;
		for(float x = -offset; x < offset; x += offset / (samples * 0.5))
		{
			for(float y = -offset; y < offset; y += offset / (samples * 0.5))
			{
				for(float z = -offset; z < offset; z += offset / (samples * 0.5))
				{
					float closestDepth = texture(shadowMapArray, vec4(fragToLight+vec3(x,y,z), i)).r;
					closestDepth *= farPlane;
					if(currentDepth - bias > closestDepth)
						tempShadow += 0.75;
				}
			}
		}
		tempShadow /= samples * samples * samples;
		shadow += tempShadow;
#else
		float closestDepth = texture(shadowMapArray, vec4(fragToLight, i)).r;
		closestDepth *= farPlane;
		shadow += currentDepth - bias > closestDepth ? 0.75 : 0.0;
#endif
	}
	return shadow;
}

vec3 getNormal(bool hasNormalMap){
	if(hasNormalMap){
		vec3 normal = texture(normalMap, fs_in.texCoords).rgb;
		normal = normalize(normal * 2.0 - 1.0);
		return normalize(fs_in.TBN * normal);
	}
	else{
		return normalize(fs_in.normal);
	}
}

void main()
{
	vec3 albedoColor;
	if(useDefaultMaterial){
		albedoColor = vec3(0.5,0.5,0.5);
	}else{
		albedoColor = texture(albedoMap, fs_in.texCoords).rgb;
	}
	albedoColor = pow(albedoColor, vec3(2.2));
	vec3 specularColor;
	if(useDefaultMaterial){
		specularColor = vec3(1.0,1.0,1.0);
	}else{
		specularColor = texture(specularMap, fs_in.texCoords).rgb;
	}
	vec3 cameraDir = normalize(cameraPos - fs_in.fragPos);
	vec3 normal = getNormal(hasNormalMap);
	vec3 ambient = 0.1 * albedoColor;
	vec3 result = vec3(0.0);
	result += calculateDirectionLight(albedoColor,specularColor,cameraDir,normal);
	result += calculatePointLight(albedoColor,specularColor,cameraDir,normal);
	result += calculateSpotLight(albedoColor,specularColor,cameraDir,normal);
#ifdef USE_ENVIRONMENT_MAPPING
	result += calculateEnvironmentMapping();
#endif
	float shadow = clamp(calculatePointLightShadow(normal) + calculateDirectionLightShadow(normal), 0.0, 1.0);
	result = (1.0 - shadow) * result;
	result += ambient;
	fragColor = vec4(result,1.0);
	float brightness = dot(fragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
	fragColor.rgb = pow(fragColor.rgb, vec3(1.0/2.2));
	if(brightness > 1.0) {
		brightColor = vec4(fragColor.rgb,1.0);
	}else{
		brightColor = vec4(vec3(0.0),1.0);
	}
}