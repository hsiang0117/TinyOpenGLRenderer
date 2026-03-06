#version 450 core

layout(location = 0) out vec4 fragColor;

in vec3 WorldPos;

layout (std140, binding = 0) uniform Matrices{
	mat4 view;
	mat4 projection;
};

uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 aabbMin;
uniform vec3 aabbMax;

uniform sampler2D depthMap;
uniform sampler3D noiseTexture;
uniform sampler2D weatherMap;
uniform vec2 resolution;
uniform float time;

vec2 rayBoxDst(vec3 boundsMin, vec3 boundsMax, vec3 rayOrigin, vec3 rayDir) {
	vec3 invRayDir = 1.0 / rayDir;
	vec3 t0 = (boundsMin - rayOrigin) * invRayDir;
	vec3 t1 = (boundsMax - rayOrigin) * invRayDir;
	vec3 tmin = min(t0, t1);
	vec3 tmax = max(t0, t1);

	float dstA = max(max(tmin.x, tmin.y), tmin.z);
	float dstB = min(min(tmax.x, tmax.y), tmax.z);

	float dstToBox = max(0.0, dstA);
	float dstInsideBox = max(0.0, dstB - dstToBox);

	return vec2(dstToBox, dstInsideBox);
}

float interleavedGradientNoise(vec2 screenPos) {
	vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

float hg(float cosAngle, float g){
	float g2 = g * g;
	return (1.0 - g2) / (4.0 * 3.1415926 * pow(1.0 + g2 - 2.0 * g * cosAngle, 1.5));
}

float phase(float cosAngle){
	float blend = 0.5;
	vec4 phaseParams = vec4(0.72, 1.0, 0.5, 1.58);
	float hgBlend = hg(cosAngle, phaseParams.x) * (1.0 - blend) + hg(cosAngle, phaseParams.y) * blend;
	return phaseParams.z + hgBlend * phaseParams.w;
}

float sampleNoise(vec3 p){
	p += vec3(time, 0.0, time);
	return texture(noiseTexture, p * 0.01).r;
}

float getEdgeFade(vec3 p, float fadeDistance) {
	float dstX = min(p.x - aabbMin.x, aabbMax.x - p.x);
	float dstZ = min(p.z - aabbMin.z, aabbMax.z - p.z);
	float edgeDist = min(dstX, dstZ);
	return smoothstep(0.0, fadeDistance, edgeDist);
}

float remap(float value, float low1, float high1, float low2, float high2) {
	return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

float getHeightGradient(float heightFraction, float cloudType) {
	float stratus = smoothstep(0.0, 0.06, heightFraction) * smoothstep(0.25, 0.12, heightFraction);
	float stratocumulus = smoothstep(0.0, 0.1, heightFraction) * smoothstep(0.55, 0.3, heightFraction);
	float cumulus = smoothstep(0.0, 0.1, heightFraction) * smoothstep(1.0, 0.6, heightFraction);
	float a = mix(stratus, stratocumulus, clamp(cloudType * 2.0, 0.0, 1.0));
	return mix(a, cumulus, clamp(cloudType * 2.0 - 1.0, 0.0, 1.0));
}

float getCloudDensity(vec3 p) {
	float baseNoise = 0.0;
	float amplitude = 0.5;
	float frequency = 1.0;
	for(int i = 0; i < 4; i++) {
		baseNoise += sampleNoise(p * frequency) * amplitude;
		amplitude *= 0.5;
		frequency *= 2.0;
	}

	vec2 weatherUv = vec2((p.x - aabbMin.x) / (aabbMax.x - aabbMin.x), (p.z - aabbMin.z) / (aabbMax.z - aabbMin.z));
	weatherUv = clamp(weatherUv, 0.0, 1.0);
	float weather = texture(weatherMap, weatherUv).r;
	float coverage = mix(0.3, 0.85, weather);

	float heightFraction = clamp((p.y - aabbMin.y) / (aabbMax.y - aabbMin.y), 0.0, 1.0);
	float heightGradient = getHeightGradient(heightFraction, 0.1);

	float baseDensity = baseNoise * heightGradient;
	baseDensity = clamp(remap(baseDensity, 1.0 - coverage, 1.0, 0.0, 1.0), 0.0, 1.0);
	if (baseDensity <= 0.0) return 0.0;

	vec3 detailPos = p + vec3(time * 0.5, 0.0, time * 0.5);
	float detailNoise = texture(noiseTexture, detailPos * 0.05).r;
	float detailModifier = mix(detailNoise, 1.0 - detailNoise, clamp(heightFraction * 5.0, 0.0, 1.0));
	baseDensity = clamp(remap(baseDensity, detailModifier * 0.15, 1.0, 0.0, 1.0), 0.0, 1.0);

	baseDensity *= getEdgeFade(p, 10.0);

	return baseDensity;
}

float getCloudDensityLight(vec3 p) {
	float density = sampleNoise(p);

	vec2 weatherUv = vec2((p.x - aabbMin.x) / (aabbMax.x - aabbMin.x), (p.z - aabbMin.z) / (aabbMax.z - aabbMin.z));
	weatherUv = clamp(weatherUv, 0.0, 1.0);
	float weather = texture(weatherMap, weatherUv).r;
	float coverage = mix(0.3, 0.85, weather);
	float heightFraction = clamp((p.y - aabbMin.y) / (aabbMax.y - aabbMin.y), 0.0, 1.0);
	float heightGradient = getHeightGradient(heightFraction, 0.1);

	float baseDensity = density * heightGradient;
	baseDensity = clamp(remap(baseDensity, 1.0 - coverage, 1.0, 0.0, 1.0), 0.0, 1.0);

	baseDensity *= getEdgeFade(p, 10.0);

	return baseDensity;
}

float lightMarch(vec3 startPos, vec3 lightDir){
	vec2 boundsInfo = rayBoxDst(aabbMin, aabbMax, startPos, lightDir);
	float maxDst = boundsInfo.y;
	float stepLen = maxDst / 8.0;
	float sumDensity = 0.0;
	vec3 testPoint = startPos;
	for(int i = 0; i< 8; i++){
		testPoint += lightDir * stepLen;
		sumDensity += max(0.0, getCloudDensityLight(testPoint) * stepLen);
	}
	float lightAttenuation = 3.0;
	float transmittance = exp(-sumDensity * lightAttenuation);
	return transmittance;
}

vec4 cloudRayMarching(vec3 rayOrigin, vec3 rayDirection, float maxDst){
	float stepLen = maxDst / 64.0;
	vec2 uv = gl_FragCoord.xy / resolution;
	float jitter = interleavedGradientNoise(gl_FragCoord.xy) * stepLen;
	vec3 testPoint = rayOrigin + rayDirection * jitter;

	float depth = texture(depthMap, uv).r;
	if (depth < 1.0) {
		vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
		mat4 invVP = inverse(projection * view);
		vec4 worldOpaquePos = invVP * ndc;
		worldOpaquePos /= worldOpaquePos.w;

		float distToOpaque = length(worldOpaquePos.xyz - cameraPos);
		float distToOrigin = length(rayOrigin - cameraPos);
		maxDst = min(maxDst, max(0.0, distToOpaque - distToOrigin));
	}

	float marchingDst = jitter;
	float transmittance = 1.0;
	vec3 lightEnergy = vec3(0.0);
	float cosAngle = dot(rayDirection, lightDir);
	float phaseVal = phase(cosAngle);

	for(int i = 0; i < 64; i++){
		if (marchingDst >= maxDst) break;

		float currentStepLen = min(stepLen, maxDst - marchingDst);
		testPoint += rayDirection * currentStepLen;
		marchingDst += currentStepLen;
		float density = getCloudDensity(testPoint);
		if (density < 0.01) continue;
		float lightTransmittance = lightMarch(testPoint, lightDir);
		float od = -log(max(lightTransmittance, 0.0001));
		float powder = 1.0 - exp(-density * 2.0);

		float accA = 1.0;
		float accB = 1.0;
		float accP = 1.0;
		vec3 totalScattering = vec3(0.0);
		for (int ms = 0; ms < 4; ms++) {
			float msTransmittance = exp(-od * accA);
			float msPhase = mix(1.0 / (4.0 * 3.1415926), phaseVal, accP);
			totalScattering += accB * msTransmittance * msPhase * lightColor;
			accA *= 0.25;
			accB *= 0.5;
			accP *= 0.5;
		}
		totalScattering *= powder;

		float heightFraction = clamp((testPoint.y - aabbMin.y) / (aabbMax.y - aabbMin.y), 0.0, 1.0);
		vec3 ambientTop = vec3(0.5, 0.6, 0.85);
		vec3 ambientBottom = vec3(0.45, 0.48, 0.55);
		vec3 ambientLight = mix(ambientBottom, ambientTop, heightFraction);
		ambientLight *= length(lightColor) * 0.25;

		vec3 currentLight = totalScattering * 10.0 + ambientLight;
		lightEnergy += density * currentStepLen * transmittance * currentLight;

		float viewAttenuation = 3.0;
		transmittance *= exp(-density * currentStepLen * viewAttenuation);
		if(transmittance < 0.01) break;
	}

	return vec4(lightEnergy, 1.0 - transmittance);
}

void main()
{
	vec3 rayDir = normalize(WorldPos - cameraPos);
	vec2 boundsInfo = rayBoxDst(aabbMin, aabbMax, cameraPos, rayDir);
	vec3 rayOrigin = cameraPos + rayDir * boundsInfo.x;
	vec4 res = cloudRayMarching(rayOrigin, rayDir, boundsInfo.y);

	fragColor = res;
}
