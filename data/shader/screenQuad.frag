#version 450 core
#include "data/shader/settings.glsl"

in vec2 texCoords;

layout (binding = 0) uniform sampler2D colorBuffer;
layout (binding = 1) uniform sampler2D blurBuffer;
layout (binding = 2) uniform sampler2D afterEffectBuffer;
uniform float exposure = 1.0;

out vec4 fragColor;

void main(){
    vec3 color = texture(colorBuffer, texCoords).rgb;
    vec3 bloom = texture(blurBuffer, texCoords).rgb;
    vec4 afterEffect = texture(afterEffectBuffer, texCoords);
    color = color * (1.0-afterEffect.a) + afterEffect.rgb;
#ifdef USE_BLOOM
    color += bloom;
#endif
#ifdef USE_HDR
    vec3 mapped = vec3(1.0) - exp(-color * exposure);
    fragColor = vec4(mapped, 1.0);
#else
	fragColor = vec4(color,1.0);
#endif
}