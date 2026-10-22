#ifndef TOGR_NOISE_TEXTURE_GENERATOR_HPP
#define TOGR_NOISE_TEXTURE_GENERATOR_HPP
#pragma once

#include "noiseVolumeData.hpp"

namespace togr {

class Texture2D;
class Texture3D;

class NoiseTextureGenerator3D
{
public:
    // Load CPU texels from data/cache/*.noise or compute them on a cache miss.
    // No GL calls; may run on workers with internally parallelized computation.
    // Disk I/O and cache writes are best-effort, non-atomic, and do not coordinate
    // concurrent callers publishing the same key; payloads have no checksum.
    static NoiseVolumeData computeWorleyNoise(int width, int height, int depth);
    // One tileable Perlin octave; the cloud shader assembles the frequency hierarchy.
    static NoiseVolumeData computePerlinNoise(int width, int height, int depth);
    // Texture-authored weather for LocalVolume. SkyLayer uses world-space weather.
    static NoiseVolumeData computeWeatherMap(int width, int height);

    // Upload a computed result as a GL texture (must run on the GL-context thread).
    static Texture3D uploadVolume3D(const NoiseVolumeData& v);
    static Texture2D uploadVolume2D(const NoiseVolumeData& v);
};

} // namespace togr

#endif
