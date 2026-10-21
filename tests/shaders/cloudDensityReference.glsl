// Step-three density reference. Only its storage stride follows CloudVolumeGpu.
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

float getCloudDensity(CloudVolume v, vec3 p) {
    if (v.densityMultiplier <= 0.0) return 0.0;
    if (v.skyLayer == 0 && (any(lessThan(p, v.aabbMin)) || any(greaterThan(p, v.aabbMax)))) return 0.0;
    float height = cloudHeight(v), bottom = cloudBottom(v);
    if (height <= 0.0 || p.y <= bottom || p.y >= bottom + height) return 0.0;
    float h = (p.y - bottom) / height;
    float heightGradient = getHeightGradient(h, v.cloudType);
    if (heightGradient <= 0.0) return 0.0;

    vec3 noisePosition = cloudNoisePosition(p, v.windVelocity, time);
    float weather;
    if (v.skyLayer != 0) weather = cloudWeather(noisePosition.xz, v.weatherScale);
    else {
        vec2 uv = clamp((noisePosition.xz - v.aabbMin.xz) / (v.aabbMax.xz - v.aabbMin.xz), 0.0, 1.0);
        weather = texture(weatherMap, uv).r;
    }
    float coverage = cloudCoverageMask(v.coverageLow, v.coverageHigh, weather);
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
    return density * coverage * getEdgeFade(v, p) * max(v.densityMultiplier, 0.0);
}

float cloudExtinction(CloudVolume v, float density) {
    return density * max(v.extinctionCoefficient, 0.0);
}
