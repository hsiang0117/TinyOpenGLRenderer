#version 450 core
#include "data/shader/settings.glsl"

in vec2 texCoords;

layout (binding = 0) uniform sampler2D hdrColorTexture;
layout (binding = 1) uniform sampler2D bloomBlurTexture;
layout (binding = 2) uniform sampler2D overlayColorTexture;
uniform float exposure = 1.0;

out vec4 fragColor;

void main(){
    vec3 color = texture(hdrColorTexture, texCoords).rgb;
    vec3 bloom = texture(bloomBlurTexture, texCoords).rgb;
    vec4 overlayColor = texture(overlayColorTexture, texCoords);
    color = color * (1.0-overlayColor.a) + overlayColor.rgb;
#ifdef USE_BLOOM
    color += bloom;
#endif
#ifdef USE_HDR
    vec3 mapped = vec3(1.0) - exp(-color * exposure);
    fragColor = vec4(mapped, 1.0);
#else
	fragColor = vec4(color,1.0);
#endif
	// HDR, bloom, and premultiplied overlays are composed in linear space.
	// Encode once, after tone mapping, for the viewport's display texture.
	fragColor.rgb = pow(max(fragColor.rgb, vec3(0.0)), vec3(1.0 / 2.2));
}
