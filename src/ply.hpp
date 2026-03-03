#ifndef PLY_HPP
#define PLY_HPP
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <future>
#include <unordered_map>
#include <atomic>

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

std::future<bool> Ply::loadFromFileAsync(const char* path) {
	loaded.store(false);
	loadProgress.store(0.0f);
	return std::async(std::launch::async, [this, path]() {
		bool result = loadFromFileImpl(path);
		loaded.store(result);
		return result;
	});
}

bool Ply::loadFromFile(const char* path) {
	loaded.store(false);
	loadProgress.store(0.0f);
	bool result = loadFromFileImpl(path);
	loaded.store(result);
	return result;
}

bool Ply::loadFromFileImpl(const char* path) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		std::cerr << "Failed to open PLY file: " << path << std::endl;
		return false;
	}

	std::string line;
	bool isAscii = false;
	int vertexCount = 0;
	std::vector<std::string> properties;
	while (std::getline(file, line)) {
		std::istringstream iss(line);
		std::string keyword;
		iss >> keyword;

		if (keyword == "format") {
			std::string format;
			iss >> format;
			isAscii = (format == "ascii");
		}
		else if (keyword == "element") {
			std::string element;
			iss >> element;
			if (element == "vertex") {
				iss >> vertexCount;
			}
		}
		else if (keyword == "property") {
			std::string type, name;
			iss >> type >> name;
			properties.push_back(name);
		}
		else if (keyword == "end_header") {
			break;
		}
	}

	if (vertexCount == 0) {
		std::cerr << "No vertices found in PLY file" << std::endl;
		return false;
	}

	std::unordered_map<std::string, int> propIndexMap;
	for (size_t i = 0; i < properties.size(); ++i) {
		propIndexMap[properties[i]] = static_cast<int>(i);
	}

	int shCoeffCount = 0;
	for (const auto& prop : properties) {
		if (prop.find("f_dc_") != std::string::npos || prop.find("f_rest_") != std::string::npos) {
			shCoeffCount++;
		}
	}

	points.clear();
	points.reserve(vertexCount);

	if (isAscii) {
		for (int i = 0; i < vertexCount; ++i) {
			if (!std::getline(file, line)) break;
			std::istringstream iss(line);
			GaussianPoint point{};
			point.shs.resize(shCoeffCount, 0.0f);

			std::vector<float> values(properties.size());
			for (size_t j = 0; j < properties.size(); ++j) {
				if (!(iss >> values[j])) break;
			}

			auto getIdx = [&](const char* name) { 
				auto it = propIndexMap.find(name); 
				return it != propIndexMap.end() ? it->second : -1; 
			};

			int idx;
			if ((idx = getIdx("x")) >= 0) point.position[0] = values[idx];
			if ((idx = getIdx("y")) >= 0) point.position[1] = values[idx];
			if ((idx = getIdx("z")) >= 0) point.position[2] = values[idx];
			if ((idx = getIdx("nx")) >= 0) point.normal[0] = values[idx];
			if ((idx = getIdx("ny")) >= 0) point.normal[1] = values[idx];
			if ((idx = getIdx("nz")) >= 0) point.normal[2] = values[idx];
			if ((idx = getIdx("scale_0")) >= 0) point.scale[0] = values[idx];
			if ((idx = getIdx("scale_1")) >= 0) point.scale[1] = values[idx];
			if ((idx = getIdx("scale_2")) >= 0) point.scale[2] = values[idx];
			if ((idx = getIdx("rot_0")) >= 0) point.rotation[0] = values[idx];
			if ((idx = getIdx("rot_1")) >= 0) point.rotation[1] = values[idx];
			if ((idx = getIdx("rot_2")) >= 0) point.rotation[2] = values[idx];
			if ((idx = getIdx("rot_3")) >= 0) point.rotation[3] = values[idx];
			if ((idx = getIdx("opacity")) >= 0) point.opacity = sigmoid(values[idx]);

			for (size_t j = 0; j < properties.size(); ++j) {
				const auto& prop = properties[j];
				if (prop.find("f_dc_") == 0) {
					int shIdx = std::stoi(prop.substr(5));
					if (shIdx < shCoeffCount) point.shs[shIdx] = values[j];
				} else if (prop.find("f_rest_") == 0) {
					int shIdx = 3 + std::stoi(prop.substr(7));
					if (shIdx < shCoeffCount) point.shs[shIdx] = values[j];
				}
			}

			points.push_back(std::move(point));
			
			if (i % 1000 == 0) {
				loadProgress.store(static_cast<float>(i) / vertexCount);
			}
		}
	}
	else {
		const size_t propertyCount = properties.size();
		std::vector<float> buffer(propertyCount * vertexCount);
		file.read(reinterpret_cast<char*>(buffer.data()), buffer.size() * sizeof(float));

		for (int i = 0; i < vertexCount; ++i) {
			GaussianPoint point{};
			point.shs.resize(shCoeffCount, 0.0f);
			
			const float* row = &buffer[i * propertyCount];

			auto getIdx = [&](const char* name) { 
				auto it = propIndexMap.find(name); 
				return it != propIndexMap.end() ? it->second : -1; 
			};

			int idx;
			if ((idx = getIdx("x")) >= 0) point.position[0] = row[idx];
			if ((idx = getIdx("y")) >= 0) point.position[1] = row[idx];
			if ((idx = getIdx("z")) >= 0) point.position[2] = row[idx];
			if ((idx = getIdx("nx")) >= 0) point.normal[0] = row[idx];
			if ((idx = getIdx("ny")) >= 0) point.normal[1] = row[idx];
			if ((idx = getIdx("nz")) >= 0) point.normal[2] = row[idx];
			if ((idx = getIdx("scale_0")) >= 0) point.scale[0] = row[idx];
			if ((idx = getIdx("scale_1")) >= 0) point.scale[1] = row[idx];
			if ((idx = getIdx("scale_2")) >= 0) point.scale[2] = row[idx];
			if ((idx = getIdx("rot_0")) >= 0) point.rotation[0] = row[idx];
			if ((idx = getIdx("rot_1")) >= 0) point.rotation[1] = row[idx];
			if ((idx = getIdx("rot_2")) >= 0) point.rotation[2] = row[idx];
			if ((idx = getIdx("rot_3")) >= 0) point.rotation[3] = row[idx];
			if ((idx = getIdx("opacity")) >= 0) point.opacity = sigmoid(row[idx]);

			for (size_t j = 0; j < properties.size(); ++j) {
				const auto& prop = properties[j];
				if (prop.find("f_dc_") == 0) {
					int shIdx = std::stoi(prop.substr(5));
					if (shIdx < shCoeffCount) point.shs[shIdx] = row[j];
				} else if (prop.find("f_rest_") == 0) {
					int shIdx = 3 + std::stoi(prop.substr(7));
					if (shIdx < shCoeffCount) point.shs[shIdx] = row[j];
				}
			}

			points.push_back(std::move(point));
			
			if (i % 1000 == 0) {
				loadProgress.store(static_cast<float>(i) / vertexCount);
			}
		}
	}

	file.close();
	loadProgress.store(1.0f);
	std::cout << "Loaded " << points.size() << " Gaussian points from " << path << std::endl;
	return true;
}
#endif // !PLY_HPP
