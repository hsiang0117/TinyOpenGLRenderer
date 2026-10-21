#version 450 core
layout(location = 0) out vec4 sampleValue;
uniform sampler3D perlinNoiseTexture;
uniform sampler3D worleyNoiseTexture;
uniform vec3 worldOrigin;
uniform vec2 worldStep;
uniform vec3 velocity;
uniform float seconds;
uniform float baseScale, warpScale, warpStrength, detailScale, weatherScale;
uniform int outputMode;
#include "data/shader/cloudNoise.glsl"

void main() {
    vec2 offset = (gl_FragCoord.xy - 0.5) * worldStep;
    vec3 world = worldOrigin + vec3(offset.x, 0.0, offset.y);
    vec3 p = cloudNoisePosition(world, velocity, seconds);
    if (outputMode == 1) {
        sampleValue = vec4(cloudWarpedPosition(p, warpScale, warpStrength) - p,
                           cloudWeather(p.xz, weatherScale));
    } else {
        sampleValue = vec4(cloudBodyNoise(perlinNoiseTexture, p, baseScale, warpScale, warpStrength),
                           cloudWeather(p.xz, weatherScale),
                           cloudDetailNoise(worleyNoiseTexture, p, detailScale),
                           texture(perlinNoiseTexture, p * baseScale).r);
    }
}
