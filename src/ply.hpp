#ifndef PLY_HPP
#define PLY_HPP
#pragma once

#include <string>
#include <vector>
#include <future>
#include <atomic>
#include <cmath>

inline float sigmoid(float x) {
	return 1.0f / (1.0f + std::exp(-x));
}

struct GaussianPoint {
	float position[3];
	float scale[3];
	float rotation[4];
	float normal[3];
	float opacity;
	std::vector<float> shs;
};

class Ply {
public:
	Ply() = default;
	~Ply() = default;

	bool loadFromFile(const char* path);
	std::future<bool> loadFromFileAsync(const char* path);
	bool isLoaded() const { return loaded.load(); }
	float getLoadProgress() const { return loadProgress.load(); }
	const std::vector<GaussianPoint>& getPoints() const { return points; }
	size_t getPointCount() const { return points.size(); }

private:
	std::vector<GaussianPoint> points;
	std::atomic<bool> loaded{false};
	std::atomic<float> loadProgress{0.0f};
	bool loadFromFileImpl(const char* path);
};

#endif // !PLY_HPP
