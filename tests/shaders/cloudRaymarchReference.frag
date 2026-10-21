#version 450 core

layout(location = 0) out vec4 fragColor;

uniform mat4 invVP;
uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 lightColor;


uniform sampler2D depthMap;
uniform sampler3D worleyNoiseTexture;
uniform sampler3D perlinNoiseTexture;
uniform sampler2D weatherMap;
uniform vec2 resolution;
uniform float time;

#include "data/shader/cloudNoise.glsl"
#include "tests/shaders/cloudDensityReference.glsl"
layout(std430, binding = 4) readonly buffer CloudVolumes { CloudVolume cloudVolumes[]; };
uniform int cloudVolumeCount;
vec2 rayBoxDst(vec3 boundsMin, vec3 boundsMax, vec3 rayOrigin, vec3 rayDir) {
	vec3 safeDir = mix(rayDir, mix(vec3(-1e-8), vec3(1e-8), greaterThanEqual(rayDir, vec3(0.0))), lessThan(abs(rayDir), vec3(1e-8)));
	vec3 invRayDir = 1.0 / safeDir;
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

// A sky layer is horizontally unbounded. Only its altitude range and tracing
// budget limit a ray; handle horizontal rays without dividing by zero.
vec2 cloudRayDst(CloudVolume v, vec3 origin, vec3 direction) {
    if (v.skyLayer == 0) return rayBoxDst(v.aabbMin, v.aabbMax, origin, direction);
    float bottom = v.layerBottom;
    float top = bottom + v.layerThickness;
    float limit = max(v.maxTraceDistance, 0.0);
    if (top <= bottom || limit <= 0.0) return vec2(0.0);
    if (abs(direction.y) < 1e-8) {
        return origin.y > bottom && origin.y < top ? vec2(0.0, limit) : vec2(0.0);
    }
    float a = (bottom - origin.y) / direction.y;
    float b = (top - origin.y) / direction.y;
    float entry = max(0.0, min(a, b));
    float exitDistance = min(limit, max(a, b));
    return vec2(entry, max(0.0, exitDistance - entry));
}

// Fade the view medium toward transparent sky near its tracing limit. Both
// extinction and scattered radiance use this density, preserving premultiplied alpha.
float cloudDistanceFade(CloudVolume v, float distanceFromCamera) {
    if (v.skyLayer == 0) return 1.0;
    float limit = max(v.maxTraceDistance, 0.0);
    if (distanceFromCamera >= limit) return 0.0;
    float start = clamp(v.distanceFadeStart, 0.0, limit);
    if (start >= limit) return 1.0;
    return 1.0 - smoothstep(start, limit, distanceFromCamera);
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

float phase(float cosAngle, float g) {
    // A normalized dual HG lobe; no arbitrary isotropic brightness floor.
    g = clamp(g, 0.0, 0.85);
    return mix(hg(cosAngle, -0.3 * g), hg(cosAngle, g), 0.85);
}

float lightOpticalDepth(vec3 p) {
    float od = 0.0;
    for (int j = 0; j < cloudVolumeCount; ++j) {
        CloudVolume v = cloudVolumes[j];
        vec2 hit = cloudRayDst(v, p, lightDir);
        if (hit.y <= 0.0) continue;
        float stepLen = hit.y / 8.0;
        for (int k = 0; k < 8; ++k) {
            vec3 q = p + lightDir * (hit.x + (float(k) + 0.5) * stepLen);
            od += cloudExtinction(v, getCloudDensity(v, q)) * stepLen;
        }
    }
    return od;
}

vec3 incidentLight(CloudVolume v, vec3 p, float od, float cosAngle) {
    float attenuation = 1.0, weight = 1.0, g = v.phaseForwardG;
    float scattering = 0.0;
    // Bounded, damped higher orders are an artistic multiple-scattering
    // approximation. Their lobes approach isotropic as their path lengths shrink.
    for (int order = 0; order < 4; ++order) {
        scattering += weight * exp(-od * attenuation) * phase(cosAngle, g);
        attenuation *= 0.35;
        weight *= 0.5 * clamp(v.multipleScattering, 0.0, 1.0) * clamp(v.scatteringAlbedo, 0.0, 1.0);
        g *= 0.5;
    }
    float h = clamp((p.y - cloudBottom(v)) / cloudHeight(v), 0.0, 1.0);
    vec3 ambient = mix(vec3(0.65, 0.72, 0.82), vec3(0.95, 0.98, 1.0), h) * max(v.ambientStrength, 0.0);
    // Engine directional lights use a diffuse irradiance / pi convention.
    return scattering * 3.1415926 * max(v.sunStrength, 0.0) * max(lightColor, vec3(0.0)) + ambient;
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    vec4 farPos = invVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec3 rayDir = normalize(farPos.xyz / farPos.w - cameraPos);
    float opaqueDistance = 1e20;
    float depth = texture(depthMap, uv).r;
    if (depth < 0.999999) {
        vec4 p = invVP * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
        opaqueDistance = length(p.xyz / p.w - cameraPos);
    }
    float cursor = opaqueDistance, end = 0.0;
    for (int j = 0; j < cloudVolumeCount; ++j) {
        vec2 hit = cloudRayDst(cloudVolumes[j], cameraPos, rayDir);
        if (hit.y <= 0.0 || hit.x >= opaqueDistance) continue;
        cursor = min(cursor, hit.x);
        end = max(end, min(hit.x + hit.y, opaqueDistance));
    }
    float transmittance = 1.0;
    vec3 energy = vec3(0.0);
    float jitter = interleavedGradientNoise(gl_FragCoord.xy);
    float cosAngle = dot(rayDir, lightDir);
    // Split at every entry/exit, jumping empty gaps. All overlapping media are
    // integrated together, so extinction and scattering do not depend on draw order.
    for (int segment = 0; segment < 2 * cloudVolumeCount + 1; ++segment) {
        if (cursor >= end || transmittance < 0.01) break;
        float boundary = end, spacing = 1e20;
        bool hasMedium = false;
        for (int j = 0; j < cloudVolumeCount; ++j) {
            CloudVolume v = cloudVolumes[j];
            vec2 hit = cloudRayDst(v, cameraPos, rayDir);
            float exitDistance = min(hit.x + hit.y, opaqueDistance);
            if (hit.y <= 0.0 || exitDistance <= cursor + 0.00001) continue;
            if (hit.x > cursor + 0.00001) { boundary = min(boundary, hit.x); continue; }
            hasMedium = true;
            boundary = min(boundary, exitDistance);
            spacing = min(spacing, hit.y / clamp(hit.y / max(v.viewStepWorldSize, 0.0001), 32.0, 96.0));
        }
        if (!hasMedium) { cursor = boundary; continue; }
        int steps = int(clamp(ceil((boundary - cursor) / spacing), 1.0, 96.0));
        float stepLen = (boundary - cursor) / float(steps);
        for (int k = 0; k < steps; ++k) {
            float rayDistance = cursor + (float(k) + jitter) * stepLen;
            vec3 p = cameraPos + rayDir * rayDistance;
            float extinction = 0.0, od = -1.0;
            vec3 emission = vec3(0.0);
            for (int j = 0; j < cloudVolumeCount; ++j) {
                CloudVolume v = cloudVolumes[j];
                float density = getCloudDensity(v, p) * cloudDistanceFade(v, rayDistance);
                float sigmaT = cloudExtinction(v, density);
                if (sigmaT <= 0.0) continue;
                if (od < 0.0) od = lightOpticalDepth(p);
                extinction += sigmaT;
                float sigmaS = sigmaT * clamp(v.scatteringAlbedo, 0.0, 1.0);
                emission += sigmaS * incidentLight(v, p, od, cosAngle);
            }
            float stepTr = exp(-extinction * stepLen);
            float integral = extinction > 0.00001 ? (1.0 - stepTr) / extinction : stepLen;
            energy += transmittance * emission * integral;
            transmittance *= stepTr;
            if (transmittance < 0.01) break;
        }
        cursor = boundary;
    }
    fragColor = vec4(energy, 1.0 - transmittance);
}
