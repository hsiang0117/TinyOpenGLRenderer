#ifndef UTILS_HPP
#define UTILS_HPP
#pragma once

#include<assimp/quaternion.h>
#include<assimp/vector3.h>
#include<assimp/matrix4x4.h>
#include<glm/glm.hpp>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include "texture.hpp"

class AssimpGLMHelpers
{
public:

	static inline glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from)
	{
		glm::mat4 to;
		//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
		to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		return to;
	}

	static inline glm::vec3 GetGLMVec(const aiVector3D& vec)
	{
		return glm::vec3(vec.x, vec.y, vec.z);
	}

	static inline glm::quat GetGLMQuat(const aiQuaternion& pOrientation)
	{
		return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
	}
};

class NoiseTextureGenerator3D 
{
public:
	static Texture3D generateWorleyNoiseTexture3D(int width, int height, int depth) {
		int numCells = 4;
		std::vector<glm::vec3> featurePoints(numCells * numCells * numCells);

		std::mt19937 gen(42);
		std::uniform_real_distribution<float> dis(0.0f, 1.0f);

		for (int z = 0; z < numCells; z++) {
			for (int y = 0; y < numCells; y++) {
				for (int x = 0; x < numCells; x++) {
					float px = x + dis(gen);
					float py = y + dis(gen);
					float pz = z + dis(gen);
					featurePoints[x + y * numCells + z * numCells * numCells] = glm::vec3(px, py, pz) / (float)numCells;
				}
			}
		}

		std::vector<unsigned char> data(width * height * depth);
		float maxDist = std::sqrt(3.0f) / numCells;

		for (int z = 0; z < depth; z++) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					glm::vec3 p = glm::vec3((float)x / width, (float)y / height, (float)z / depth);

					int cx = std::floor(p.x * numCells);
					int cy = std::floor(p.y * numCells);
					int cz = std::floor(p.z * numCells);

					float minDistance = 1.0f;

					for (int dz = -1; dz <= 1; dz++) {
						for (int dy = -1; dy <= 1; dy++) {
							for (int dx = -1; dx <= 1; dx++) {
								int nx = (cx + dx + numCells) % numCells;
								int ny = (cy + dy + numCells) % numCells;
								int nz = (cz + dz + numCells) % numCells;

								glm::vec3 fp = featurePoints[nx + ny * numCells + nz * numCells * numCells];

								float distX = std::abs(p.x - fp.x);
								float distY = std::abs(p.y - fp.y);
								float distZ = std::abs(p.z - fp.z);

								distX = std::min(distX, 1.0f - distX);
								distY = std::min(distY, 1.0f - distY);
								distZ = std::min(distZ, 1.0f - distZ);

								float dist = std::sqrt(distX * distX + distY * distY + distZ * distZ);
								minDistance = std::min(minDistance, dist);
							}
						}
					}

					float noiseValue = 1.0f - (minDistance / maxDist);
					noiseValue = std::max(0.0f, std::min(1.0f, noiseValue)); // clamp to 0-1
					data[x + y * width + z * width * height] = (unsigned char)(noiseValue * 255.0f);
				}
			}
		}

		Texture3D texture(width, height, depth, GL_REPEAT, GL_LINEAR, GL_RED, GL_RED, GL_UNSIGNED_BYTE);
		texture.subImage3D(0, 0, 0, width, height, depth, data.data());

		return texture;
	}

	static Texture3D generatePerlinNoiseTexture3D(int width, int height, int depth) {
		std::vector<unsigned char> data(width * height * depth);

		std::vector<int> p(512);
		std::mt19937 gen(58);
		std::vector<int> permutation(256);
		for (int i = 0; i < 256; ++i) permutation[i] = i;
		std::shuffle(permutation.begin(), permutation.end(), gen);
		for (int i = 0; i < 256; ++i) {
			p[i] = permutation[i];
			p[i + 256] = permutation[i];
		}

		auto fade = [](float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); };
		auto lerp = [](float t, float a, float b) { return a + t * (b - a); };
		auto grad = [](int hash, float x, float y, float z) {
			int h = hash & 15;
			float u = h < 8 ? x : y;
			float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
			return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
		};

		float baseFrequency = 4.0f;

		auto noise = [&](float x, float y, float z, int per) {
			int xi = (int)std::floor(x);
			int yi = (int)std::floor(y);
			int zi = (int)std::floor(z);

			float xf = x - std::floor(x);
			float yf = y - std::floor(y);
			float zf = z - std::floor(z);

			float u = fade(xf);
			float v = fade(yf);
			float w = fade(zf);

			auto wrap = [](int val, int per) {
				int r = val % per;
				return r < 0 ? r + per : r;
			};

			int x0 = wrap(xi, per), x1 = wrap(xi + 1, per);
			int y0 = wrap(yi, per), y1 = wrap(yi + 1, per);
			int z0 = wrap(zi, per), z1 = wrap(zi + 1, per);

			int aaa = p[p[p[x0] + y0] + z0];
			int aba = p[p[p[x0] + y1] + z0];
			int aab = p[p[p[x0] + y0] + z1];
			int abb = p[p[p[x0] + y1] + z1];
			int baa = p[p[p[x1] + y0] + z0];
			int bba = p[p[p[x1] + y1] + z0];
			int bab = p[p[p[x1] + y0] + z1];
			int bbb = p[p[p[x1] + y1] + z1];

			float res = lerp(w,
				lerp(v,
					lerp(u, grad(aaa, xf, yf, zf), grad(baa, xf - 1.0f, yf, zf)),
					lerp(u, grad(aba, xf, yf - 1.0f, zf), grad(bba, xf - 1.0f, yf - 1.0f, zf))),
				lerp(v,
					lerp(u, grad(aab, xf, yf, zf - 1.0f), grad(bab, xf - 1.0f, yf, zf - 1.0f)),
					lerp(u, grad(abb, xf, yf - 1.0f, zf - 1.0f), grad(bbb, xf - 1.0f, yf - 1.0f, zf - 1.0f))));
			return (res + 1.0f) / 2.0f;
		};
		for (int z = 0; z < depth; z++) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					float nx = (float)x / width;
					float ny = (float)y / height;
					float nz = (float)z / depth;

					float n = 0.0f;
					float amplitude = 0.5f;
					float freq = baseFrequency;
					for (int i = 0; i < 4; i++) {
						n += noise(nx * freq, ny * freq, nz * freq, (int)freq) * amplitude;
						amplitude *= 0.5f;
						freq *= 2.0f;
					}
					n = (n - 0.5f) * 2.5f + 0.5f;
					n = std::max(0.0f, std::min(1.0f, n));
					data[x + y * width + z * width * height] = (unsigned char)(n * 255.0f);
				}
			}
		}

		Texture3D texture(width, height, depth, GL_REPEAT, GL_LINEAR, GL_RED, GL_RED, GL_UNSIGNED_BYTE);
		texture.subImage3D(0, 0, 0, width, height, depth, data.data());

		return texture;
	}

	static Texture2D generateWeatherMapTexture2D(int width, int height) {
		std::vector<unsigned char> data(width * height);

		std::vector<int> p(512);
		std::mt19937 gen(60);
		std::vector<int> permutation(256);
		for (int i = 0; i < 256; ++i) permutation[i] = i;
		std::shuffle(permutation.begin(), permutation.end(), gen);
		for (int i = 0; i < 256; ++i) {
			p[i] = permutation[i];
			p[i + 256] = permutation[i];
		}

		auto fade = [](float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); };
		auto lerp = [](float t, float a, float b) { return a + t * (b - a); };
		auto grad = [](int hash, float x, float y) {
			int h = hash & 15;
			float u = h < 8 ? x : y;
			float v = h < 4 ? y : h == 12 || h == 14 ? x : 0.0f;
			return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
		};

		auto noise = [&](float x, float y) {
			int X = (int)std::floor(x) & 255;
			int Y = (int)std::floor(y) & 255;

			x -= std::floor(x);
			y -= std::floor(y);

			float u = fade(x);
			float v = fade(y);

			int A = p[X] + Y, B = p[X + 1] + Y;

			float res = lerp(v, lerp(u, grad(p[A], x, y),
				grad(p[B], x - 1.0f, y)),
				lerp(u, grad(p[A + 1], x, y - 1.0f),
					grad(p[B + 1], x - 1.0f, y - 1.0f)));
			return (res + 1.0f) / 2.0f;
		};

		auto fbm = [&](float x, float y) {
			float value = 0.0f;
			float amplitude = 0.5f;
			float frequency = 3.0f;
			for (int i = 0; i < 4; i++) {
				value += noise(x * frequency, y * frequency) * amplitude;
				amplitude *= 0.5f;
				frequency *= 2.0f;
			}
			return value;
		};

		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float nx = (float)x / width;
				float ny = (float)y / height;

				float n = fbm(nx, ny);
				n = std::max(0.0f, std::min(1.0f, n));
				data[x + y * width] = (unsigned char)(n * 255.0f);
			}
		}

		Texture2D texture(width, height, GL_REPEAT, GL_LINEAR, GL_RED, GL_RED, GL_UNSIGNED_BYTE);
		texture.subImage2D(0, 0, width, height, data.data());

		return texture;
	}
};

#endif