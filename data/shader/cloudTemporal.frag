#version 450 core
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragDepth;
uniform sampler2D currentColor, currentDepth, historyColor, historyDepth;
uniform mat4 invVP, currentVP, previousVP;
uniform vec3 cameraPos, cameraDisplacement, windDisplacement;
uniform int historyValid;

void main() {
    ivec2 size = textureSize(currentColor, 0);
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec2 uv = gl_FragCoord.xy / vec2(size);
    vec4 current = texelFetch(currentColor, pixel, 0);
    vec2 depth = texelFetch(currentDepth, pixel, 0).rg;
    fragColor = current; fragDepth = depth;
    if (historyValid == 0 || current.a <= 0.0001 || depth.x <= 0.0) return;
    vec4 farPos = invVP * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec3 direction = normalize(farPos.xyz / farPos.w - cameraPos);
    // Project camera-relative vectors (w=0 removes the view translation), then
    // apply only the motion to this pixel's UV. A stationary view is exactly
    // stationary even when far-away world coordinates lose float precision.
    vec3 currentPosition = direction * depth.x;
    vec3 previousPosition = currentPosition + cameraDisplacement - windDisplacement;
    vec4 projected = previousVP * vec4(previousPosition, 0.0);
    vec4 currentProjected = currentVP * vec4(currentPosition, 0.0);
    if (projected.w <= 0.0 || currentProjected.w <= 0.0) return;
    vec2 previousUV = uv + (projected.xy / projected.w - currentProjected.xy / currentProjected.w) * 0.5;
    vec2 margin = 0.5 / vec2(size);
    if (any(lessThan(previousUV, margin)) || any(greaterThan(previousUV, 1.0 - margin))) return;
    vec2 oldDepth = texture(historyDepth, previousUV).rg;
    float expectedDistance = length(previousPosition);
    float tolerance = max(8.0, expectedDistance * 0.08);
    if (oldDepth.x <= 0.0 || abs(oldDepth.x - expectedDistance) > tolerance || oldDepth.y < expectedDistance) return;
    // Geometry disocclusion needs its own test: the representative cloud point
    // alone says nothing about a foreground object that moved across the ray.
    bool opaque = depth.y < 1e19, wasOpaque = oldDepth.y < 1e19;
    if (opaque != wasOpaque) return;
    if (opaque) {
        float expectedOpaque = length(direction * depth.y + cameraDisplacement);
        if (abs(oldDepth.y - expectedOpaque) > max(1.0, expectedOpaque * 0.02)) return;
    }
    vec4 low = current, high = current;
    for (int y = -1; y <= 1; ++y) for (int x = -1; x <= 1; ++x) {
        vec4 neighbor = texelFetch(currentColor, clamp(pixel + ivec2(x, y), ivec2(0), size - 1), 0);
        low = min(low, neighbor); high = max(high, neighbor);
    }
    // Keep history inside today's premultiplied neighborhood to stop cloud
    // edges leaving trails. Alpha is accumulated with the same weight as RGB.
    vec4 history = clamp(texture(historyColor, previousUV), low, high);
    float confidence = 1.0 - smoothstep(tolerance * 0.25, tolerance, abs(oldDepth.x - expectedDistance));
    fragColor = mix(current, history, 0.85 * confidence);
}
