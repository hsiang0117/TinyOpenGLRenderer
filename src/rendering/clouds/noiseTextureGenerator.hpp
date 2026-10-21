#ifndef NOISE_TEXTURE_GENERATOR_HPP
#define NOISE_TEXTURE_GENERATOR_HPP
#pragma once

#include "noiseVolumeData.hpp"

class Texture2D;
class Texture3D;

class NoiseTextureGenerator3D
{
public:
	// Convenience: compute and upload synchronously (build-time / main thread).
	static Texture3D generateWorleyNoiseTexture3D(int width, int height, int depth);
	static Texture3D generatePerlinNoiseTexture3D(int width, int height, int depth);
	static Texture2D generateWeatherMapTexture2D(int width, int height);

	// Pure CPU compute (no GL calls, thread-safe); safe to run on a worker thread.
	// Internally parallelized across CPU cores. Backed by a disk cache
	// (data/cache/*.noise): on a hit the data is read directly instead of recomputed.
	static NoiseVolumeData computeWorleyNoise(int width, int height, int depth);
	// One tileable Perlin octave; the cloud shader assembles the frequency hierarchy.
	static NoiseVolumeData computePerlinNoise(int width, int height, int depth);
	// Texture-authored weather for LocalVolume. SkyLayer uses world-space weather.
	static NoiseVolumeData computeWeatherMap(int width, int height);

	// Upload a computed result as a GL texture (must run on the GL-context thread).
	static Texture3D uploadVolume3D(const NoiseVolumeData& v);
	static Texture2D uploadVolume2D(const NoiseVolumeData& v);
};

#endif
