// All cloud fields share one advected world-space domain. Positive velocity
// moves the pattern in that direction; applying time once keeps octaves rigid.
vec3 cloudNoisePosition(vec3 worldPosition, vec3 velocity, float seconds) {
    return worldPosition - velocity * seconds;
}

// Integer lattice hashing avoids the short period of a repeated weather texture.
// Quintic interpolation makes the field continuous across positive/negative cells.
float cloudLatticeValue(ivec2 cell, uint seed) {
    uvec2 bits = uvec2(cell);
    uint h = (bits.x * 0x8da6b343u) ^ (bits.y * 0xd8163841u) ^ seed;
    h ^= h >> 16u; h *= 0x7feb352du;
    h ^= h >> 15u; h *= 0x846ca68bu;
    h ^= h >> 16u;
    return float(h >> 8u) / 16777215.0;
}

float cloudValueNoise(vec2 p, uint seed) {
    ivec2 cell = ivec2(floor(p));
    vec2 f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float a = cloudLatticeValue(cell, seed);
    float b = cloudLatticeValue(cell + ivec2(1, 0), seed);
    float c = cloudLatticeValue(cell + ivec2(0, 1), seed);
    float d = cloudLatticeValue(cell + ivec2(1, 1), seed);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Macro coverage is separate from the 3D cloud body. No texture UV wrap or
// camera-relative origin is involved, so distant regions get different weather.
float cloudWeather(vec2 position, float scale) {
    const mat2 rotation = mat2(0.8, 0.6, -0.6, 0.8);
    vec2 q = position * scale;
    float weather = 0.625 * cloudValueNoise(q, 0x684bc21fu);
    q = rotation * q * 2.17 + vec2(13.37, -8.91);
    weather += 0.25 * cloudValueNoise(q, 0x02e5be93u);
    q = rotation * q * 2.17 + vec2(-5.73, 17.19);
    weather += 0.125 * cloudValueNoise(q, 0x967a889bu);
    return weather;
}

// Conservative range over a rectangle. Quintic interpolation is monotonic in
// each coordinate inside one cell, so extrema lie at its four corners. Crossing
// a cell returns the full range rather than risking a missed cloud silhouette.
vec2 cloudValueNoiseBounds(vec2 lo, vec2 hi, uint seed) {
    ivec2 cell = ivec2(floor(lo));
    if (any(notEqual(cell, ivec2(floor(hi))))) return vec2(0.0, 1.0);
    vec2 a = fract(lo), b = fract(hi);
    a = a * a * a * (a * (a * 6.0 - 15.0) + 10.0);
    b = b * b * b * (b * (b * 6.0 - 15.0) + 10.0);
    float c00 = cloudLatticeValue(cell, seed);
    float c10 = cloudLatticeValue(cell + ivec2(1, 0), seed);
    float c01 = cloudLatticeValue(cell + ivec2(0, 1), seed);
    float c11 = cloudLatticeValue(cell + ivec2(1, 1), seed);
    vec4 corners = vec4(
        mix(mix(c00, c10, a.x), mix(c01, c11, a.x), a.y),
        mix(mix(c00, c10, b.x), mix(c01, c11, b.x), a.y),
        mix(mix(c00, c10, a.x), mix(c01, c11, a.x), b.y),
        mix(mix(c00, c10, b.x), mix(c01, c11, b.x), b.y));
    return clamp(vec2(min(min(corners.x, corners.y), min(corners.z, corners.w)) - 0.00001,
                      max(max(corners.x, corners.y), max(corners.z, corners.w)) + 0.00001), 0.0, 1.0);
}

vec2 cloudWeatherBounds(vec2 lo, vec2 hi, float scale) {
    const mat2 rotation = mat2(0.8, 0.6, -0.6, 0.8);
    const mat2 absRotation = mat2(0.8, 0.6, 0.6, 0.8);
    vec2 center = (lo + hi) * 0.5 * scale;
    vec2 extent = (hi - lo) * 0.5 * abs(scale) + vec2(0.00001) + abs(center) * 0.000001;
    vec2 result = 0.625 * cloudValueNoiseBounds(center - extent, center + extent, 0x684bc21fu);
    center = rotation * center * 2.17 + vec2(13.37, -8.91);
    extent = absRotation * extent * 2.17 + vec2(0.00001) + abs(center) * 0.000001;
    result += 0.25 * cloudValueNoiseBounds(center - extent, center + extent, 0x02e5be93u);
    center = rotation * center * 2.17 + vec2(-5.73, 17.19);
    extent = absRotation * extent * 2.17 + vec2(0.00001) + abs(center) * 0.000001;
    result += 0.125 * cloudValueNoiseBounds(center - extent, center + extent, 0x967a889bu);
    return clamp(result + vec2(-0.00001, 0.00001), 0.0, 1.0);
}

vec3 cloudWarpedPosition(vec3 position, float scale, float strength) {
    vec2 q = position.xz * scale;
    vec2 warp = vec2(cloudValueNoise(q, 0x1b873593u),
                     cloudValueNoise(q, 0x85ebca6bu)) * 2.0 - 1.0;
    // Independent X/Z channels; the height coordinate is never displaced.
    return position + vec3(warp.x, 0.0, warp.y) * strength;
}

// Orthogonal rotation preserves scale while breaking axis-aligned tiling. Each
// octave changes orientation and uses a non-integer frequency ratio. The source
// texture contains ONE Perlin octave; FBM is assembled here, once, in [0,0.9375].
const mat3 CLOUD_NOISE_ROTATION = mat3(
     0.804035156,  0.510957923, -0.304055042,
    -0.400103467,  0.843228125,  0.359003545,
     0.439823468, -0.166997994,  0.882421094);

float cloudBodyNoise(sampler3D source, vec3 position, float baseScale,
                     float warpScale, float warpStrength) {
    vec3 warped = cloudWarpedPosition(position, warpScale, warpStrength);
    vec3 q = CLOUD_NOISE_ROTATION * (warped * baseScale) + vec3(0.17, 0.31, 0.73);
    float value = 0.0, amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        value += texture(source, q).r * amplitude;
        q = CLOUD_NOISE_ROTATION * q * 2.03 + vec3(0.37, -0.29, 0.53);
        amplitude *= 0.5;
    }
    return value;
}

float cloudDetailNoise(sampler3D source, vec3 position, float scale) {
    vec3 q = CLOUD_NOISE_ROTATION * (position * scale) + vec3(0.61, 0.19, 0.47);
    return texture(source, q).r;
}
