#pragma once
#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>

// Matches CloudVolume in cloudRaymarch.frag (std430, binding 4).
struct alignas(16) CloudVolumeGpu {
    glm::vec3 aabbMin; float cloudType;
    glm::vec3 aabbMax; float coverageLow;
    float coverageHigh, baseNoiseScale, warpScale, warpStrength;
    float detailScale, detailErosionStrength, extinctionCoefficient, densityMultiplier;
    float scatteringAlbedo, multipleScattering, edgeFadeDistance, viewStepWorldSize;
    glm::vec3 windVelocity; std::int32_t skyLayer = 0;
    float layerBottom, layerThickness, maxTraceDistance, distanceFadeStart;
    float weatherScale, ambientStrength, phaseForwardG, sunStrength;
    std::int32_t viewMaxSteps = 96, lightSteps = 8, emptyStepMultiplier = 4, padding = 0;
    float regionScale = 0.0004f, regionCoverage = 1.0f, densityVariation = 0.0f, heightVariation = 0.0f;
};
static_assert(sizeof(CloudVolumeGpu) == 160);
static_assert(offsetof(CloudVolumeGpu, windVelocity) == 80);
static_assert(offsetof(CloudVolumeGpu, skyLayer) == 92);
static_assert(offsetof(CloudVolumeGpu, layerBottom) == 96);
static_assert(offsetof(CloudVolumeGpu, weatherScale) == 112);
static_assert(offsetof(CloudVolumeGpu, ambientStrength) == 116);
static_assert(offsetof(CloudVolumeGpu, phaseForwardG) == 120);
static_assert(offsetof(CloudVolumeGpu, sunStrength) == 124);
static_assert(offsetof(CloudVolumeGpu, viewMaxSteps) == 128);
static_assert(offsetof(CloudVolumeGpu, regionScale) == 144);
