// std430 layout, shared with CloudVolumeGpu (160 bytes).
struct CloudVolume {
    vec3 aabbMin; float cloudType;
    vec3 aabbMax; float coverageLow;
    float coverageHigh, baseNoiseScale, warpScale, warpStrength;
    float detailScale, detailErosionStrength, extinctionCoefficient, densityMultiplier;
    float scatteringAlbedo, multipleScattering, edgeFadeDistance, viewStepWorldSize;
    vec3 windVelocity; int skyLayer;
    float layerBottom, layerThickness, maxTraceDistance, distanceFadeStart;
    float weatherScale, ambientStrength, phaseForwardG, sunStrength;
    int viewMaxSteps, lightSteps, emptyStepMultiplier, padding;
    float regionScale, regionCoverage, densityVariation, heightVariation;
};

float cloudBottom(CloudVolume v) {
    return v.skyLayer != 0 ? v.layerBottom : v.aabbMin.y;
}

float cloudHeight(CloudVolume v) {
    return v.skyLayer != 0 ? v.layerThickness : v.aabbMax.y - v.aabbMin.y;
}

float getHeightGradient(float h, float cloudType) {
    float stratus = smoothstep(0.0, 0.06, h) * (1.0 - smoothstep(0.12, 0.25, h));
    float stratocumulus = smoothstep(0.0, 0.1, h) * (1.0 - smoothstep(0.3, 0.55, h));
    float cumulus = smoothstep(0.0, 0.1, h) * (1.0 - smoothstep(0.6, 1.0, h));
    float a = mix(stratus, stratocumulus, clamp(cloudType * 2.0, 0.0, 1.0));
    return mix(a, cumulus, clamp(cloudType * 2.0 - 1.0, 0.0, 1.0));
}

float getEdgeFade(CloudVolume v, vec3 p) {
    if (v.skyLayer != 0) return 1.0;
    vec3 center = (v.aabbMin + v.aabbMax) * 0.5;
    vec3 extent = (v.aabbMax - v.aabbMin) * 0.5;
    float radius = min(extent.x, extent.z);
    return 1.0 - smoothstep(max(0.0, radius - max(v.edgeFadeDistance, 0.0001)),
                            radius, length(p.xz - center.xz));
}

float cloudCoverageMask(float low, float high, float weather) {
    // Macro coverage gates a WHOLE column, before any 3D fetch. Lower coverage
    // creates clear sky instead of only shaving a little density from every cell.
    float coverage = clamp(mix(low, high, weather), 0.0, 1.0);
    return smoothstep(0.4, 0.6, coverage);
}

const uint CLOUD_REGION_SEED = 0x98b35ac1u;

float cloudRegionMask(float amount, float weather) {
    if (amount >= 1.0) return 1.0;
    if (amount <= 0.0) return 0.0;
    float threshold = 1.0 - amount;
    // Shrink the transition near the endpoints instead of popping entire
    // nearly-clear / nearly-overcast regions when the slider reaches 0 or 1.
    float feather = min(0.12, min(amount, 1.0 - amount));
    return smoothstep(threshold - feather, threshold + feather, weather);
}

// Broad weather is a separate scale from the smaller cloud coverage/body.
// One column has coherent coverage, density and height through its whole depth.
vec3 cloudRegionProperties(CloudVolume v, vec2 noiseXZ) {
    if (v.skyLayer == 0) return vec3(1.0);
    vec2 q = noiseXZ * v.regionScale;
    float coverage = v.regionCoverage >= 1.0 ? 1.0 :
        cloudRegionMask(v.regionCoverage, cloudValueNoise(q, CLOUD_REGION_SEED));
    if (coverage <= 0.0) return vec3(0.0, 1.0, 1.0);
    float shape = 1.0;
    if (v.densityVariation > 0.0 || v.heightVariation > 0.0)
        shape = smoothstep(0.1, 0.9, cloudValueNoise(q * 2.17 + vec2(19.31, -7.73), 0x51633e2du));
    return vec3(coverage, mix(1.0, mix(0.2, 1.0, shape), clamp(v.densityVariation, 0.0, 1.0)),
        mix(1.0 - clamp(v.heightVariation, 0.0, 0.8), 1.0, shape));
}

bool cloudBlockEmpty(CloudVolume v, vec3 first, vec3 last) {
    if (v.densityMultiplier <= 0.0 || v.extinctionCoefficient <= 0.0 || cloudHeight(v) <= 0.0) return true;
    vec3 margin = vec3(0.001) + max(abs(first), abs(last)) * 0.000001;
    vec3 lo = min(first, last) - margin, hi = max(first, last) + margin;
    if (v.skyLayer == 0 && (any(lessThan(hi, v.aabbMin)) || any(greaterThan(lo, v.aabbMax)))) return true;
    float h0 = (lo.y - cloudBottom(v)) / cloudHeight(v);
    float h1 = (hi.y - cloudBottom(v)) / cloudHeight(v);
    if (h1 <= 0.0 || h0 >= 1.0) return true;
    // Local tops can be lower than the global layer bound. Include every
    // possible normalized height before proving a segment empty.
    float minHeightFraction = v.skyLayer != 0 ? 1.0 - clamp(v.heightVariation, 0.0, 0.8) : 1.0;
    h0 = min(h0, h0 / minHeightFraction);
    h1 = max(h1, h1 / minHeightFraction);
    // Every height profile increases up to h=0.1 and never increases afterward.
    float maxHeight = max(getHeightGradient(h0, v.cloudType), getHeightGradient(h1, v.cloudType));
    maxHeight = max(maxHeight, getHeightGradient(clamp(0.1, h0, h1), v.cloudType));
    if (maxHeight + 0.00001 <= mix(0.12, 0.48, clamp(v.cloudType, 0.0, 1.0))) return true;
    if (max(v.coverageLow, v.coverageHigh) <= 0.4) return true;
    if (v.skyLayer == 0) return false;
    if (v.regionCoverage <= 0.0) return true;
    vec2 advectedLo = cloudNoisePosition(lo, v.windVelocity, time).xz;
    vec2 advectedHi = cloudNoisePosition(hi, v.windVelocity, time).xz;
    if (v.regionCoverage < 1.0) {
        vec2 center = (advectedLo + advectedHi) * 0.5 * v.regionScale;
        vec2 extent = (advectedHi - advectedLo) * 0.5 * abs(v.regionScale) + vec2(0.00001) + abs(center) * 0.000001;
        vec2 region = cloudValueNoiseBounds(center - extent, center + extent, CLOUD_REGION_SEED);
        if (region.y + 0.00001 <= 1.0 - v.regionCoverage - 0.12) return true;
    }
    if (min(v.coverageLow, v.coverageHigh) >= 0.4) return false;
    vec2 weather = cloudWeatherBounds(advectedLo, advectedHi, v.weatherScale);
    float bound = mix(v.coverageLow, v.coverageHigh, v.coverageHigh >= v.coverageLow ? weather.y : weather.x);
    return bound + 0.00001 <= 0.4;
}

float getCloudDensity(CloudVolume v, vec3 p) {
    if (v.densityMultiplier <= 0.0) return 0.0;
    if (v.skyLayer == 0 && (any(lessThan(p, v.aabbMin)) || any(greaterThan(p, v.aabbMax)))) return 0.0;
    float height = cloudHeight(v), bottom = cloudBottom(v);
    if (height <= 0.0 || p.y <= bottom || p.y >= bottom + height) return 0.0;
    float h = (p.y - bottom) / height;
    float heightGradient = getHeightGradient(h, v.cloudType);
    float minBodyFloor = mix(0.12, 0.48, clamp(v.cloudType, 0.0, 1.0));
    // Lowering a local top only moves h upward. Above the common profile peak,
    // the global-height envelope remains an upper bound, before regional noise.
    if ((v.skyLayer == 0 || v.heightVariation <= 0.0 || h >= 0.1) && heightGradient <= minBodyFloor) return 0.0;
    vec3 noisePosition = cloudNoisePosition(p, v.windVelocity, time);
    vec3 region = cloudRegionProperties(v, noisePosition.xz);
    if (region.x <= 0.0) return 0.0;
    h /= region.z;
    if (h >= 1.0) return 0.0;
    if (region.z < 1.0) heightGradient = getHeightGradient(h, v.cloudType);
    // Body noise is bounded by one after normalization. No weather or 3D fetch
    // can produce density when even full coverage lies below the body threshold.
    if (heightGradient <= minBodyFloor) return 0.0;

    float weather;
    if (v.skyLayer != 0) weather = cloudWeather(noisePosition.xz, v.weatherScale);
    else {
        vec2 uv = clamp((noisePosition.xz - v.aabbMin.xz) / (v.aabbMax.xz - v.aabbMin.xz), 0.0, 1.0);
        weather = texture(weatherMap, uv).r;
    }
    float coverage = cloudCoverageMask(v.coverageLow, v.coverageHigh, weather) * region.x;
    if (coverage <= 0.0) return 0.0;

    // Normalize the FBM's amplitude sum and cut a silhouette with the height
    // envelope. Stratus has a softer body threshold than billowing cumulus.
    float body = cloudBodyNoise(perlinNoiseTexture, noisePosition, v.baseNoiseScale, v.warpScale, v.warpStrength) / 0.9375;
    float bodyFloor = mix(0.66, mix(0.12, 0.48, clamp(v.cloudType, 0.0, 1.0)), coverage);
    float density = clamp((body * heightGradient - bodyFloor) / (1.0 - bodyFloor), 0.0, 1.0);
    if (density <= 0.0) return 0.0;
    if (v.detailErosionStrength > 0.0) {
        float detail = cloudDetailNoise(worleyNoiseTexture, noisePosition, v.detailScale);
        float erosion = mix(detail, 1.0 - detail, clamp(h * 5.0, 0.0, 1.0)) * clamp(v.detailErosionStrength, 0.0, 1.0);
        density = clamp((density - erosion) / max(1.0 - erosion, 0.0001), 0.0, 1.0);
    }
    return density * coverage * region.y * getEdgeFade(v, p) * max(v.densityMultiplier, 0.0);
}

float cloudExtinction(CloudVolume v, float density) {
    return density * max(v.extinctionCoefficient, 0.0);
}
