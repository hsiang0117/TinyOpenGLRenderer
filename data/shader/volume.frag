#version 450 core

layout(location = 0) out vec4 fragColor;

uniform mat4 invVP;
uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 aabbMin;
uniform vec3 aabbMax;

uniform sampler2D depthMap;
uniform sampler3D worleyNoiseTexture;
uniform sampler3D perlinNoiseTexture;
uniform sampler2D weatherMap;
uniform vec2 resolution;
uniform float time;

// Per-volume cloud parameters (from VolumeMarkerComponent). Defaults below reproduce
// the previous hardcoded constants exactly.
uniform float cloudType;
uniform float coverageLow;
uniform float coverageHigh;
uniform float baseNoiseScale;
uniform float warpScale;
uniform float warpStrength;
uniform float detailScale;
uniform float detailErosionStrength;
uniform float lightAttenuation;
uniform float densityGain;
uniform float viewAttenuation;
uniform float powderStrength;
uniform vec3 windVelocity;
uniform float edgeFadeDistance;
uniform float viewStepWorldSize;

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
	float t = 1.0 + g2 - 2.0 * g * cosAngle;
	return (1.0 - g2) / (4.0 * 3.1415926 * t * sqrt(t));
}

float phase(float cosAngle){
	float blend = 0.5;
	vec4 phaseParams = vec4(0.72, -0.5, 0.5, 1.58);
	float hgBlend = hg(cosAngle, phaseParams.x) * (1.0 - blend) + hg(cosAngle, phaseParams.y) * blend;
	return phaseParams.z + hgBlend * phaseParams.w;
}

float sampleNoise(vec3 p){
	p += windVelocity * time;
	return texture(perlinNoiseTexture, p * baseNoiseScale).r;
}

float getEdgeFade(vec3 p, float fadeDistance) {
	vec3 center = (aabbMin + aabbMax) * 0.5;
	vec3 extent = (aabbMax - aabbMin) * 0.5;
	float radius = min(extent.x, extent.z);
	vec2 offset = p.xz - center.xz;
	float dist = length(offset);
	return smoothstep(radius, radius - fadeDistance, dist);
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

// Shared cloud silhouette field: domain warp + 4-octave Perlin FBM, shaped by the
// analytic height gradient and weather coverage. This is the base density field
// common to BOTH the view march and the light (shadow) march, so self-shadowing is
// sampled from the same medium that is rendered. Returns the post-coverage density
// in [0,1] (0 in empty space, via the same provably-safe early-outs as before).
// Outputs heightFraction so callers reuse it without a recompute.
//
// Standard Perlin-Worley layout: the low/mid-frequency Perlin FBM here gives the
// coherent cloud body; getCloudDensity then erodes the edges with high-frequency
// Worley. Cheap empty-space rejection runs first: heightGradient is analytic (0
// fetches), coverage costs 1 weather fetch. The FBM below has maximum value equal to
// the sum of its octave amplitudes (0.5+0.25+0.125+0.0625 = 0.9375); since both noise
// textures are normalized to [0,1], if even that maximum cannot clear the coverage
// remap floor (1 - coverage), the result is provably 0 for all noise, so the 6 warp +
// FBM fetches are skipped with no change in output.
float cloudBaseDensity(vec3 p, out float heightFraction) {
	heightFraction = clamp((p.y - aabbMin.y) / (aabbMax.y - aabbMin.y), 0.0, 1.0);
	float heightGradient = getHeightGradient(heightFraction, cloudType);
	if (heightGradient <= 0.0) return 0.0;

	vec2 weatherUv = vec2((p.x - aabbMin.x) / (aabbMax.x - aabbMin.x), (p.z - aabbMin.z) / (aabbMax.z - aabbMin.z));
	weatherUv = clamp(weatherUv, 0.0, 1.0);
	float weather = texture(weatherMap, weatherUv).r;
	float coverage = mix(coverageLow, coverageHigh, weather);

	// NOISE_MAX is the FBM's max value (sum of the 4 octave amplitudes). It stays a
	// literal because the octave count below is fixed; the empty-space early-out is
	// only conservative against that exact bound.
	const float NOISE_MAX = 0.9375;
	if (NOISE_MAX * heightGradient <= 1.0 - coverage) return 0.0;

	vec3 warp = vec3(
		texture(perlinNoiseTexture, p * warpScale).r,
		0.0,
		texture(perlinNoiseTexture, p * warpScale + vec3(43.0, 17.0, 59.0)).r
	);
	vec3 warpedP = p + (warp - 0.5) * warpStrength;

	float baseNoise = 0.0;
	float amplitude = 0.5;
	float frequency = 1.0;
	vec3 offset = vec3(0.0);
	for(int i = 0; i < 4; i++) {
		baseNoise += sampleNoise((warpedP + offset) * frequency) * amplitude;
		amplitude *= 0.5;
		frequency *= 2.0;
		offset += vec3(31.416, -47.853, 12.679);
	}

	float baseDensity = baseNoise * heightGradient;
	return clamp(remap(baseDensity, 1.0 - coverage, 1.0, 0.0, 1.0), 0.0, 1.0);
}

float getCloudDensity(vec3 p) {
	float heightFraction;
	float baseDensity = cloudBaseDensity(p, heightFraction);
	if (baseDensity <= 0.0) return 0.0;

	// View-only high-frequency detail erosion (carves the visible cloud edges).
	// Uses the high-frequency Worley texture sampled at a finer world scale than the
	// base, giving cauliflower-style fringes rather than a uniform density scale.
	vec3 detailPos = p + windVelocity * time;
	float detailNoise = texture(worleyNoiseTexture, detailPos * detailScale).r;
	float detailModifier = mix(detailNoise, 1.0 - detailNoise, clamp(heightFraction * 5.0, 0.0, 1.0));
	baseDensity = clamp(remap(baseDensity, detailModifier * detailErosionStrength, 1.0, 0.0, 1.0), 0.0, 1.0);

	baseDensity *= getEdgeFade(p, edgeFadeDistance);

	return baseDensity;
}

float getCloudDensityLight(vec3 p) {
	// Shadow density is the SAME silhouette field as the view march, minus the
	// high-frequency detail erosion (a legitimate cheap-light approximation, since
	// shadowing is low frequency). It previously used a single raw noise tap, which —
	// through the identical coverage remap — produced a structurally different field
	// at a different magnitude, placing self-shadows that did not match the visible
	// cloud. Sharing cloudBaseDensity makes the light field a true band-limited subset.
	float heightFraction;
	float baseDensity = cloudBaseDensity(p, heightFraction);
	baseDensity *= getEdgeFade(p, edgeFadeDistance);
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
	float transmittance = exp(-sumDensity * lightAttenuation);
	return transmittance;
}

vec4 cloudRayMarching(vec3 rayOrigin, vec3 rayDirection, float maxDst, mat4 invVP){
	// World-space step count: keep the sample spacing roughly constant (~2 world
	// units, which resolves the finest significant noise octave) instead of always
	// 64 steps regardless of box size. A ~128-unit traversal still yields 64 steps;
	// thinner boxes spend fewer steps at the same spacing, very large boxes get more
	// (up to 96) instead of undersampling. Clamped to [32, 96]; the constant 96
	// upper loop bound below keeps the GLSL loop portable/unrollable. Changing the
	// count is a quadrature-resolution change only (Beer-Lambert integrates the same),
	// so it affects banding, not overall brightness.
	int numSteps = int(clamp(maxDst / viewStepWorldSize, 32.0, 96.0));
	float stepLen = maxDst / float(numSteps);
	vec2 uv = gl_FragCoord.xy / resolution;
	float jitter = interleavedGradientNoise(gl_FragCoord.xy) * stepLen;
	vec3 testPoint = rayOrigin + rayDirection * jitter;

	float depth = texture(depthMap, uv).r;
	if (depth < 0.999) {
		vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
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

	for(int i = 0; i < 96; i++){
		if (i >= numSteps) break;
		if (marchingDst >= maxDst) break;

		float currentStepLen = min(stepLen, maxDst - marchingDst);
		testPoint += rayDirection * currentStepLen;
		marchingDst += currentStepLen;
		float density = getCloudDensity(testPoint);
		if (density < 0.01) continue;
		float lightTransmittance = lightMarch(testPoint, lightDir);
		float od = -log(max(lightTransmittance, 0.0001));
		float powder = 1.0 - exp(-density * powderStrength);

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

		vec3 currentLight = (totalScattering + ambientLight) * densityGain;
		lightEnergy += density * currentStepLen * transmittance * currentLight;

		transmittance *= exp(-density * currentStepLen * viewAttenuation);
		if(transmittance < 0.01) break;
	}

	return vec4(lightEnergy, 1.0 - transmittance);
}

void main()
{
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 clipFar = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
	vec4 worldFar = invVP * clipFar;
	worldFar /= worldFar.w;
	vec3 rayDir = normalize(worldFar.xyz - cameraPos);

	vec2 boundsInfo = rayBoxDst(aabbMin, aabbMax, cameraPos, rayDir);
	if (boundsInfo.y <= 0.0) {
		fragColor = vec4(0.0);
		return;
	}
	vec3 rayOrigin = cameraPos + rayDir * boundsInfo.x;
	vec4 res = cloudRayMarching(rayOrigin, rayDir, boundsInfo.y, invVP);

	fragColor = res;
}
