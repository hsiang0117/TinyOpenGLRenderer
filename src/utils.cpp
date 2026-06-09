#include "utils.hpp"
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <thread>
#include <fstream>
#include <filesystem>
#include <string>
#include <functional>
#include <cstdint>
#include <cstring>
#include <initializer_list>

// Split an outer loop over [0,total) across threads; body(begin,end) handles a range.
static void parallelFor(int total, const std::function<void(int, int)>& body) {
	unsigned hw = std::thread::hardware_concurrency();
	int numThreads = std::max(1u, hw);
	numThreads = std::min(numThreads, total);
	if (numThreads <= 1) {
		body(0, total);
		return;
	}
	std::vector<std::thread> threads;
	int chunk = (total + numThreads - 1) / numThreads;
	for (int t = 0; t < numThreads; t++) {
		int begin = t * chunk;
		int end = std::min(begin + chunk, total);
		if (begin >= end) break;
		threads.emplace_back(body, begin, end);
	}
	for (auto& th : threads) th.join();
}

// ---------- disk cache ----------
static std::string cachePath(const std::string& key) {
	return "data/cache/" + key + ".noise";
}

// Self-describing cache header. The old format stored raw bytes and validated only
// by file size, so changing a generator's seed/frequency/cell-count (without changing
// dimensions) would silently load a stale .noise file. The header binds the body to
// the dimensions AND a hash of the generator parameters; loadCache rejects any
// mismatch and regenerates. Bumping kCacheVersion invalidates every cache at once.
// (Old headerless files are rejected too: their first bytes won't match the magic.)
namespace {
	constexpr uint32_t kCacheMagic = 0x4E534543;   // 'CESN' little-endian tag
	constexpr uint32_t kCacheVersion = 1;

	struct NoiseCacheHeader {
		uint32_t magic;
		uint32_t version;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
		uint32_t reserved;
		uint64_t paramHash;
	};
	static_assert(sizeof(NoiseCacheHeader) == 32, "NoiseCacheHeader must be 32 bytes with no padding");

	// FNV-1a over the bit patterns of the supplied params. double exactly represents
	// the small int/float constants we feed it, so this captures seed/frequency/cells.
	uint64_t hashParams(std::initializer_list<double> vals) {
		uint64_t h = 1469598103934665603ull;
		for (double v : vals) {
			uint64_t bits = 0;
			std::memcpy(&bits, &v, sizeof(bits));
			h ^= bits;
			h *= 1099511628211ull;
		}
		return h;
	}

	NoiseCacheHeader makeHeader(int w, int h, int d, uint64_t paramHash) {
		NoiseCacheHeader hdr{};
		hdr.magic = kCacheMagic;
		hdr.version = kCacheVersion;
		hdr.width = (uint32_t)w;
		hdr.height = (uint32_t)h;
		hdr.depth = (uint32_t)d;
		hdr.reserved = 0;
		hdr.paramHash = paramHash;
		return hdr;
	}

	bool headerMatches(const NoiseCacheHeader& a, const NoiseCacheHeader& b) {
		return a.magic == b.magic && a.version == b.version &&
			a.width == b.width && a.height == b.height && a.depth == b.depth &&
			a.paramHash == b.paramHash;
	}
}

static bool loadCache(const std::string& key, const NoiseCacheHeader& expected, std::vector<unsigned char>& out) {
	std::ifstream f(cachePath(key), std::ios::binary);
	if (!f) return false;
	NoiseCacheHeader hdr{};
	if (!f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr))) return false;
	if (!headerMatches(hdr, expected)) return false;
	const size_t total = (size_t)expected.width * expected.height * expected.depth;
	out.resize(total);
	if (!f.read(reinterpret_cast<char*>(out.data()), total)) return false;
	// Reject a file that is larger than header+body (corrupt/stale): a successful
	// extra read means the byte count does not match the declared dimensions.
	char extra;
	if (f.read(&extra, 1)) return false;
	return true;
}

static void saveCache(const std::string& key, const NoiseCacheHeader& header, const std::vector<unsigned char>& data) {
	std::error_code ec;
	std::filesystem::create_directories("data/cache", ec);
	std::ofstream f(cachePath(key), std::ios::binary);
	if (f) {
		f.write(reinterpret_cast<const char*>(&header), sizeof(header));
		f.write(reinterpret_cast<const char*>(data.data()), data.size());
	}
}

// ---------- CPU compute ----------
NoiseVolumeData NoiseTextureGenerator3D::computeWorleyNoise(int width, int height, int depth) {
	NoiseVolumeData result;
	result.width = width; result.height = height; result.depth = depth;
	const std::string key = "worley_" + std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(depth);
	const size_t total = (size_t)width * height * depth;

	const int numCells = 4;
	const unsigned seed = 42;
	NoiseCacheHeader header = makeHeader(width, height, depth, hashParams({ (double)numCells, (double)seed }));
	if (loadCache(key, header, result.data)) return result;

	std::vector<glm::vec3> featurePoints(numCells * numCells * numCells);
	std::mt19937 gen(seed);
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

	result.data.resize(total);
	float maxDist = std::sqrt(3.0f) / numCells;

	parallelFor(depth, [&](int zBegin, int zEnd) {
		for (int z = zBegin; z < zEnd; z++) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					glm::vec3 p = glm::vec3((float)x / width, (float)y / height, (float)z / depth);
					int cx = (int)std::floor(p.x * numCells);
					int cy = (int)std::floor(p.y * numCells);
					int cz = (int)std::floor(p.z * numCells);
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
					noiseValue = std::max(0.0f, std::min(1.0f, noiseValue));
					result.data[x + y * width + z * width * height] = (unsigned char)(noiseValue * 255.0f);
				}
			}
		}
	});

	saveCache(key, header, result.data);
	return result;
}

NoiseVolumeData NoiseTextureGenerator3D::computePerlinNoise(int width, int height, int depth) {
	NoiseVolumeData result;
	result.width = width; result.height = height; result.depth = depth;
	const std::string key = "perlin_" + std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(depth);
	const size_t total = (size_t)width * height * depth;

	const unsigned seed = 58;
	const float baseFrequency = 4.0f;
	const int octaves = 4;
	const float contrast = 2.5f;
	NoiseCacheHeader header = makeHeader(width, height, depth,
		hashParams({ (double)seed, (double)baseFrequency, (double)octaves, (double)contrast }));
	if (loadCache(key, header, result.data)) return result;

	std::vector<int> p(512);
	std::mt19937 gen(seed);
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

	result.data.resize(total);
	parallelFor(depth, [&](int zBegin, int zEnd) {
		for (int z = zBegin; z < zEnd; z++) {
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					float nx = (float)x / width;
					float ny = (float)y / height;
					float nz = (float)z / depth;
					float n = 0.0f;
					float amplitude = 0.5f;
					float freq = baseFrequency;
					for (int i = 0; i < octaves; i++) {
						n += noise(nx * freq, ny * freq, nz * freq, (int)freq) * amplitude;
						amplitude *= 0.5f;
						freq *= 2.0f;
					}
					n = (n - 0.5f) * contrast + 0.5f;
					n = std::max(0.0f, std::min(1.0f, n));
					result.data[x + y * width + z * width * height] = (unsigned char)(n * 255.0f);
				}
			}
		}
	});

	saveCache(key, header, result.data);
	return result;
}

NoiseVolumeData NoiseTextureGenerator3D::computeWeatherMap(int width, int height) {
	NoiseVolumeData result;
	result.width = width; result.height = height; result.depth = 1;
	const std::string key = "weather_" + std::to_string(width) + "x" + std::to_string(height);
	const size_t total = (size_t)width * height;

	const unsigned seed = 60;
	const float baseFrequency = 3.0f;
	const int octaves = 4;
	NoiseCacheHeader header = makeHeader(width, height, 1,
		hashParams({ (double)seed, (double)baseFrequency, (double)octaves }));
	if (loadCache(key, header, result.data)) return result;

	std::vector<int> p(512);
	std::mt19937 gen(seed);
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
		float frequency = baseFrequency;
		for (int i = 0; i < octaves; i++) {
			value += noise(x * frequency, y * frequency) * amplitude;
			amplitude *= 0.5f;
			frequency *= 2.0f;
		}
		return value;
	};

	result.data.resize(total);
	parallelFor(height, [&](int yBegin, int yEnd) {
		for (int y = yBegin; y < yEnd; y++) {
			for (int x = 0; x < width; x++) {
				float nx = (float)x / width;
				float ny = (float)y / height;
				float n = fbm(nx, ny);
				n = std::max(0.0f, std::min(1.0f, n));
				result.data[x + y * width] = (unsigned char)(n * 255.0f);
			}
		}
	});

	saveCache(key, header, result.data);
	return result;
}

// ---------- upload as GL texture (main thread only) ----------
Texture3D NoiseTextureGenerator3D::uploadVolume3D(const NoiseVolumeData& v) {
	Texture3D texture(v.width, v.height, v.depth, GL_REPEAT, GL_LINEAR, GL_RED, GL_RED, GL_UNSIGNED_BYTE);
	texture.subImage3D(0, 0, 0, v.width, v.height, v.depth, v.data.data());
	return texture;
}

Texture2D NoiseTextureGenerator3D::uploadVolume2D(const NoiseVolumeData& v) {
	Texture2D texture(v.width, v.height, GL_REPEAT, GL_LINEAR, GL_RED, GL_RED, GL_UNSIGNED_BYTE);
	texture.subImage2D(0, 0, v.width, v.height, v.data.data());
	return texture;
}

// ---------- synchronous convenience ----------
Texture3D NoiseTextureGenerator3D::generateWorleyNoiseTexture3D(int width, int height, int depth) {
	return uploadVolume3D(computeWorleyNoise(width, height, depth));
}

Texture3D NoiseTextureGenerator3D::generatePerlinNoiseTexture3D(int width, int height, int depth) {
	return uploadVolume3D(computePerlinNoise(width, height, depth));
}

Texture2D NoiseTextureGenerator3D::generateWeatherMapTexture2D(int width, int height) {
	return uploadVolume2D(computeWeatherMap(width, height));
}
