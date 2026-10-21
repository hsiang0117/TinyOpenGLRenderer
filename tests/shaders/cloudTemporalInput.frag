#version 450 core
layout(location = 0) out vec4 color;
layout(location = 1) out vec2 depth;
uniform mat4 invVP;
uniform vec3 cameraPos, velocity;
uniform float seconds, noiseAmplitude;
uniform int noisePhase, clearCloud, foreground;
uniform vec2 resolution;
void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    vec4 farPos = invVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec3 ray = normalize(farPos.xyz / farPos.w - cameraPos);
    float distance = (200.0 - cameraPos.y) / ray.y;
    bool opaque = foreground != 0 && uv.x < 0.5;
    if (clearCloud != 0 || opaque || distance <= 0.0) {
        color = vec4(0.0); depth = vec2(-1.0, opaque ? 10.0 : 1e20); return;
    }
    vec3 p = cameraPos + ray * distance - velocity * seconds;
    vec3 radiance = vec3(0.3 + 0.08 * sin(p.x * 0.02),
                         0.4 + 0.07 * cos(p.z * 0.018),
                         0.5 + 0.05 * sin((p.x + p.z) * 0.015));
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    float noise = ((pixel.x + pixel.y + noisePhase) % 2 == 0 ? 1.0 : -1.0) * noiseAmplitude;
    color = vec4((radiance + noise) * 0.6, 0.6);
    depth = vec2(distance, 1e20);
}
