#version 450 core
layout(location = 0) out vec4 sampleValue;
uniform sampler3D perlinNoiseTexture;
uniform sampler3D worleyNoiseTexture;
uniform sampler2D weatherMap;
uniform float time;
uniform float sampleHeight;
uniform vec2 worldOrigin, worldStep;
#include "data/shader/cloudNoise.glsl"
#include "data/shader/cloudDensity.glsl"
layout(std430, binding = 4) readonly buffer CloudVolumes { CloudVolume cloudVolumes[]; };
void main() {
    CloudVolume v = cloudVolumes[0];
    vec2 xz = worldOrigin + (gl_FragCoord.xy - 0.5) * worldStep;
    vec3 p = vec3(xz.x, v.layerBottom + sampleHeight * v.layerThickness, xz.y);
    float opticalDepth = 0.0;
    for (int k = 0; k < 32; ++k) {
        vec3 q = vec3(xz.x, v.layerBottom + (float(k) + 0.5) * v.layerThickness / 32.0, xz.y);
        opticalDepth += cloudExtinction(v, getCloudDensity(v, q)) * v.layerThickness / 32.0;
    }
    vec2 noiseXZ = cloudNoisePosition(vec3(xz.x, 0.0, xz.y), v.windVelocity, time).xz;
    float coverage = cloudCoverageMask(v.coverageLow, v.coverageHigh, cloudWeather(noiseXZ, v.weatherScale))
        * cloudRegionProperties(v, noiseXZ).x;
    sampleValue = vec4(getCloudDensity(v, p), 1.0 - exp(-opticalDepth), coverage, opticalDepth);
}
