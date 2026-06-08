#version 450 core

// Depth-aware bilateral upsample of the half-resolution cloud raymarch result
// into the full-resolution after-effect target. Clouds are low frequency, so they
// are marched at half res (4x fewer fragments); this pass reconstructs the full-res
// image. A plain bilinear upscale would bleed background cloud over foreground
// geometry silhouettes (and vice versa). To avoid that, each of the 4 bilinear
// taps is weighted by how well the depth that tap saw matches this pixel's depth.

in vec2 texCoords;
out vec4 fragColor;

uniform sampler2D cloudTexture;   // half-res raymarch: rgb = scattered light, a = coverage
uniform sampler2D depthTexture;   // full-res scene depth (same texture the march sampled)
uniform vec2 cloudTexSize;        // half-res target dimensions, in texels
uniform float nearPlane;
uniform float farPlane;

float linearizeDepth(float d) {
	float ndc = d * 2.0 - 1.0;
	return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - ndc * (farPlane - nearPlane));
}

void main() {
	vec2 invSize = 1.0 / cloudTexSize;

	// Continuous half-res texel coordinate of this full-res pixel, and the 4
	// surrounding texel centers with their bilinear weights.
	vec2 st = texCoords * cloudTexSize - 0.5;
	vec2 base = floor(st);
	vec2 f = st - base;

	float bw[4];
	bw[0] = (1.0 - f.x) * (1.0 - f.y);
	bw[1] = f.x * (1.0 - f.y);
	bw[2] = (1.0 - f.x) * f.y;
	bw[3] = f.x * f.y;

	vec2 offs[4];
	offs[0] = vec2(0.0, 0.0);
	offs[1] = vec2(1.0, 0.0);
	offs[2] = vec2(0.0, 1.0);
	offs[3] = vec2(1.0, 1.0);

	// Keep taps inside the valid texel-center range so neither texture samples its
	// (clamp/border) edge: cloudTexture is CLAMP_TO_EDGE, depth is CLAMP_TO_BORDER.
	vec2 minUv = 0.5 * invSize;
	vec2 maxUv = 1.0 - 0.5 * invSize;

	float refLin = linearizeDepth(texture(depthTexture, texCoords).r);

	// dw is always > 0 (so sumW > 0): on a flat surface every tap matches and this
	// reduces to plain bilinear; across a depth discontinuity the mismatched taps
	// are strongly suppressed and the upscale snaps to the matching side.
	const float sharpness = 2.0;
	vec4 sumColor = vec4(0.0);
	float sumW = 0.0;
	for (int i = 0; i < 4; i++) {
		vec2 texelUv = clamp((base + offs[i] + 0.5) * invSize, minUv, maxUv);
		float lowLin = linearizeDepth(texture(depthTexture, texelUv).r);
		float dw = 1.0 / (1.0 + abs(lowLin - refLin) * sharpness);
		float w = bw[i] * dw;
		sumColor += texture(cloudTexture, texelUv) * w;
		sumW += w;
	}

	fragColor = sumColor / max(sumW, 1e-5);
}
