#version 450 core
layout(location = 0) out vec4 result;
uniform sampler3D worleyNoiseTexture, perlinNoiseTexture;
uniform sampler2D weatherMap;
uniform float time;
uniform vec3 origin, blockDelta;
uniform int outputMode;
#include "data/shader/cloudNoise.glsl"
#include "data/shader/cloudDensity.glsl"
layout(std430, binding = 4) readonly buffer Medium { CloudVolume volumes[]; };
void main() {
    CloudVolume v = volumes[0];
    vec3 first = origin + vec3(gl_FragCoord.x * 19.37, 0.0, gl_FragCoord.y * 23.19);
    vec3 last = first + blockDelta;
    if (outputMode == 0) {
        vec2 a = cloudNoisePosition(min(first, last), v.windVelocity, time).xz;
        vec2 b = cloudNoisePosition(max(first, last), v.windVelocity, time).xz;
        vec2 bounds = cloudWeatherBounds(a, b, v.weatherScale);
        float over = -1.0, under = -1.0;
        for (int y = 0; y <= 8; ++y) for (int x = 0; x <= 8; ++x) {
            float weather = cloudWeather(mix(a, b, vec2(x, y) / 8.0), v.weatherScale);
            over = max(over, weather - bounds.y); under = max(under, bounds.x - weather);
        }
        result = vec4(over, under, bounds);
    } else {
        bool empty = cloudBlockEmpty(v, first, last);
        float maximum = 0.0;
        for (int i = 0; i <= 32; ++i) maximum = max(maximum, getCloudDensity(v, mix(first, last, float(i) / 32.0)));
        result = vec4(empty ? maximum : 0.0, empty ? 1.0 : 0.0, maximum, 1.0);
    }
}
