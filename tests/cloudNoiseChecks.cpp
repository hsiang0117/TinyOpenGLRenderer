#include "rendering/clouds/noiseTextureGenerator.hpp"
#include "rendering/clouds/cloudVolume.hpp"
#include "scene/component.hpp"
#include "rendering/opengl/shader.hpp"
#include "rendering/opengl/texture.hpp"
#include "rendering/opengl/glBuffer.hpp"
#include "rendering/renderUtils.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

using Samples = std::vector<glm::vec4>;
double meanDifference(const Samples& a, const Samples& b, int channel) {
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) sum += std::abs(a[i][channel] - b[i][channel]);
    return sum / a.size();
}

double correlation(const Samples& a, int channelA, const Samples& b, int channelB) {
    double meanA = 0, meanB = 0;
    for (std::size_t i = 0; i < a.size(); ++i) { meanA += a[i][channelA]; meanB += b[i][channelB]; }
    meanA /= a.size(); meanB /= b.size();
    double covariance = 0, varianceA = 0, varianceB = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double x = a[i][channelA] - meanA, y = b[i][channelB] - meanB;
        covariance += x * y; varianceA += x * x; varianceB += y * y;
    }
    check(varianceA / a.size() > 0.001 && varianceB / b.size() > 0.001,
        "Noise correlation fixture has insufficient variation");
    return covariance / std::sqrt(varianceA * varianceB);
}
}

// Called with the render-correctness test's current debug GL context. The probe
// includes the production GLSL helpers; no CPU copy of the new shader is tested.
void runCloudNoiseChecks() {
    const auto lattice = NoiseTextureGenerator3D::computePerlinNoise(16, 16, 16);
    for (int z = 0; z < 16; z += 4) for (int y = 0; y < 16; y += 4) for (int x = 0; x < 16; x += 4)
        check(lattice.data[x + y * 16 + z * 256] == 127,
            "Perlin source contains baked FBM or loaded the obsolete FBM cache");
    check(NoiseTextureGenerator3D::computePerlinNoise(16, 16, 16).data == lattice.data,
        "Perlin cache changed the generated source texture");

    auto perlin = NoiseTextureGenerator3D::uploadVolume3D(NoiseTextureGenerator3D::computePerlinNoise(128, 128, 128));
    auto worley = NoiseTextureGenerator3D::uploadVolume3D(NoiseTextureGenerator3D::computeWorleyNoise(128, 128, 128));
    Shader shader("data/shader/fullscreen.vert", "tests/shaders/cloudNoiseProbe.frag");
    check(shader.isValid(), "Cloud noise probe failed to compile");
    constexpr int size = 128;
    Texture2D output(size, size, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    FrameBuffer fbo; fbo.init(); fbo.bind(); fbo.attachTexture2D(output, GL_COLOR_ATTACHMENT0);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glViewport(0, 0, size, size);
    perlin.use(GL_TEXTURE0); worley.use(GL_TEXTURE1); shader.use();
    shader.setInt("perlinNoiseTexture", 0); shader.setInt("worleyNoiseTexture", 1);
    shader.setFloat("baseScale", 0.01f); shader.setFloat("warpScale", 0.003f);
    shader.setFloat("warpStrength", 25); shader.setFloat("detailScale", 0.04f);
    shader.setFloat("weatherScale", 0.0006f); shader.setFloat("seconds", 0);
    shader.setVec3("velocity", glm::vec3(0)); shader.setInt("outputMode", 0);
    const glm::vec3 origin(-3100.25f, 175.75f, -2400.5f);
    shader.setVec2("worldStep", glm::vec2(47.31f, 37.47f));
    auto draw = [&](glm::vec3 position) {
        shader.setVec3("worldOrigin", position); RenderHelpers::drawFullscreenQuad();
        Samples samples(size * size); glReadPixels(0, 0, size, size, GL_RGBA, GL_FLOAT, samples.data());
        for (const auto& p : samples) for (int c = 0; c < 4; ++c)
            check(std::isfinite(p[c]), "Cloud noise produced NaN or infinity");
        return samples;
    };
    const auto reference = draw(origin);
    for (const auto& p : reference) {
        check(p.r >= 0 && p.r <= 0.937501f, "FBM exceeded the conservative empty-space bound");
        check(p.g >= 0 && p.g <= 1 && p.b >= 0 && p.b <= 1, "Weather/detail noise outside [0,1]");
    }
    // Positive control: raw GL_REPEAT still repeats at 100 units. The combined
    // field must not inherit that tile, the old warp/FBM period, or the weather tile.
    auto rawTile = draw(origin + glm::vec3(100, 0, 0));
    check(correlation(reference, 3, rawTile, 3) > 0.9999,
        "Periodic-texture control failed to detect a repeated tile");
    std::filesystem::create_directories("x64/RenderChecks");
    std::ofstream metrics("x64/RenderChecks/cloud-noise-correlations.csv");
    metrics << "axis,offset,body_correlation,weather_correlation,detail_correlation\n";
    for (int axis : {0, 2}) for (float distance : {25.f, 100.f, 1000.f, 5000.f}) {
        glm::vec3 offset(0); offset[axis] = distance;
        auto shifted = draw(origin + offset);
        const double body = correlation(reference, 0, shifted, 0);
        const double weather = correlation(reference, 1, shifted, 1);
        const double detail = correlation(reference, 2, shifted, 2);
        metrics << (axis == 0 ? 'X' : 'Z') << ',' << distance << ',' << body << ',' << weather << ',' << detail << '\n';
        if (distance >= 100) check(std::abs(body) < 0.9, "Cloud body still inherits an old XZ repeat period");
        if (distance == 5000) check(std::abs(weather) < 0.9, "Sky weather still repeats at the old 5000-unit boundary");
        if (distance == 25) check(std::abs(detail) < 0.9, "Cloud detail still repeats on a 25-unit XZ grid");
    }
    // Rotation/frequency separation must work even when warp is disabled.
    shader.setFloat("warpStrength", 0); auto unwarped = draw(origin);
    for (int axis : {0, 2}) {
        glm::vec3 offset(0); offset[axis] = 1000;
        check(std::abs(correlation(unwarped, 0, draw(origin + offset), 0)) < 0.9,
            "Disabling warp restored periodic body FBM");
    }
    shader.setFloat("warpStrength", 25);

    shader.setInt("outputMode", 1); auto warped = draw(origin);
    for (const auto& p : warped) {
        check(p.g == 0, "Domain warp displaced the height coordinate");
        check(std::abs(p.r) <= 25.001f && std::abs(p.b) <= 25.001f, "Domain warp exceeded its world-space strength");
    }
    check(std::abs(correlation(warped, 0, warped, 2)) < 0.85, "XZ domain warp channels remain correlated");
    shader.setInt("outputMode", 0);

    shader.setFloat("weatherScale", 0.0012f); auto largerWeatherScale = draw(origin);
    check(meanDifference(reference, largerWeatherScale, 0) < 0.000001 &&
          meanDifference(reference, largerWeatherScale, 2) < 0.000001,
          "WeatherScale changed the body or detail frequency");
    check(meanDifference(reference, largerWeatherScale, 1) > 0.01, "WeatherScale did not change the macro cloud field");
    shader.setFloat("weatherScale", 0.0006f);

    const glm::vec3 velocity(3.5f, 0, -2.25f); const float seconds = 37.25f;
    shader.setVec3("velocity", velocity); shader.setFloat("seconds", seconds);
    auto translated = draw(origin + velocity * seconds), animated = draw(origin);
    for (int c = 0; c < 3; ++c) check(meanDifference(reference, translated, c) < 0.0003,
        "Weather, warp, body or detail slipped during wind advection");
    check(meanDifference(reference, animated, 0) > 0.02, "Nonzero wind did not animate the cloud field");
    shader.setVec3("velocity", glm::vec3(0)); shader.setFloat("seconds", 0);

    shader.setVec2("worldStep", glm::vec2(0));
    for (float boundary : {-5000.f, -1000.f, -100.f, 0.f, 100.f, 1000.f, 5000.f}) {
        auto left = draw(glm::vec3(boundary - 0.0001f, 175.75f, 17.3f));
        auto right = draw(glm::vec3(boundary + 0.0001f, 175.75f, 17.3f));
        for (int c = 0; c < 3; ++c) check(std::abs(left[0][c] - right[0][c]) < 0.004f,
            "Cloud noise has a discontinuity at a positive/negative world boundary");
    }
    fbo.unbind(); glUseProgram(0);
    std::cout << "PASS: single-octave cache, XZ noise periods, independent warp, fixed height, weather scale, rigid wind, and continuity\n";

    Shader densityShader("data/shader/fullscreen.vert", "tests/shaders/cloudDensityProbe.frag");
    check(densityShader.isValid(), "Cloud density probe failed to compile");
    densityShader.use(); densityShader.setInt("perlinNoiseTexture", 0); densityShader.setInt("worleyNoiseTexture", 1);
    densityShader.setInt("weatherMap", 2); densityShader.setFloat("time", 0); densityShader.setFloat("sampleHeight", 0.4f);
    const glm::vec2 densityOrigin(-3000.25f, -2400.5f);
    densityShader.setVec2("worldOrigin", densityOrigin); densityShader.setVec2("worldStep", glm::vec2(47.31f, 37.47f));
    Texture2D weather(1, 1, GL_REPEAT, GL_NEAREST, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    const unsigned char white = 255; weather.subImage2D(0, 0, 1, 1, &white); weather.use(GL_TEXTURE2);
    ShaderStorageBuffer medium; medium.init(); fbo.bind();
    auto drawDensity = [&](const CloudVolumeComponent& v) {
        CloudVolumeGpu gpu{glm::vec3(0), v.cloudType, glm::vec3(0), v.coverageLow,
            v.coverageHigh, v.baseNoiseScale, v.warpScale, v.warpStrength,
            v.detailScale, v.detailErosionStrength, v.extinctionCoefficient, v.densityMultiplier,
            v.scatteringAlbedo, v.multipleScattering, v.edgeFadeDistance, v.viewStepWorldSize,
            v.windVelocity, 1, v.layerBottom, v.layerThickness, v.maxTraceDistance, v.distanceFadeStart,
            v.weatherScale, v.ambientStrength, v.phaseForwardG, v.sunStrength,
            v.viewMaxSteps, v.lightSteps, v.emptyStepMultiplier, 0,
            v.regionScale, v.regionCoverage, v.densityVariation, v.heightVariation};
        medium.bind(); medium.bufferBase(4); medium.bufferData(sizeof(gpu), &gpu); medium.unbind();
        RenderHelpers::drawFullscreenQuad(); Samples samples(size * size);
        glReadPixels(0, 0, size, size, GL_RGBA, GL_FLOAT, samples.data());
        for (const auto& p : samples) {
            for (int c = 0; c < 4; ++c) check(std::isfinite(p[c]), "Cloud density probe produced nonfinite values");
            check(p.r >= 0 && p.r <= v.densityMultiplier + 0.0001f && p.g >= 0 && p.g <= 1 && p.a >= 0,
                "Density, optical depth or opacity outside its valid range");
        }
        return samples;
    };
    CloudVolumeComponent v;
    const auto shaped = drawDensity(v);
    std::ofstream densityMetrics("x64/RenderChecks/cloud-preset-density.csv");
    densityMetrics << "preset,clear_column_fraction,mean_vertical_opacity\n";
    const char* names[] = {"cumulus", "stratocumulus", "overcast"};
    for (int preset = 0; preset < 3; ++preset) {
        v.applyPreset(static_cast<CloudVolumeComponent::Preset>(preset));
        const auto samples = drawDensity(v); double clear = 0, meanOpacity = 0;
        for (const auto& p : samples) { clear += p.g < 0.01f ? 1 : 0; meanOpacity += p.g; }
        clear /= samples.size(); meanOpacity /= samples.size();
        densityMetrics << names[preset] << ',' << clear << ',' << meanOpacity << '\n';
        if (preset == 0) check(clear > 0.1 && clear < 0.95 && meanOpacity > 0.02,
            "Scattered cumulus must contain both clear columns and visible clouds");
        if (preset == 2) check(clear < 0.1 && meanOpacity > 0.5,
            "Overcast preset must cover most of the sky with visible density");
    }
    v.applyPreset(CloudVolumeComponent::Preset::ScatteredCumulus);
    v.densityMultiplier *= 2; const auto denser = drawDensity(v);
    for (std::size_t i = 0; i < shaped.size(); ++i) {
        check(std::abs(denser[i].r - shaped[i].r * 2) < 0.0001f && std::abs(denser[i].a - shaped[i].a * 2) < 0.0001f,
            "DensityMultiplier must scale the medium and shadow optical depth together");
    }
    v.applyPreset(CloudVolumeComponent::Preset::ScatteredCumulus);
    v.coverageLow += 0.2f; v.coverageHigh = 1; const auto covered = drawDensity(v);
    for (std::size_t i = 0; i < shaped.size(); ++i)
        check(covered[i].g + 0.00001f >= shaped[i].g, "Increasing coverage removed cloud columns");
    v.applyPreset(CloudVolumeComponent::Preset::ScatteredCumulus);
    v.detailErosionStrength = 0; const auto uneroded = drawDensity(v);
    for (std::size_t i = 0; i < shaped.size(); ++i)
        check(uneroded[i].g + 0.00001f >= shaped[i].g, "Detail erosion added shadow density");

    v.applyPreset(CloudVolumeComponent::Preset::ScatteredCumulus);
    v.regionCoverage = 1; v.densityVariation = v.heightVariation = 0;
    const auto uniformRegions = drawDensity(v);
    std::ofstream regionMetrics("x64/RenderChecks/cloud-region-distribution.csv");
    regionMetrics << "window_x,window_z,uniform_opacity,regional_opacity\n";
    auto windowMeans = [&](const Samples& samples) {
        std::vector<double> means;
        for (int z = 0; z < size; z += 16) for (int x = 0; x < size; x += 16) {
            double mean = 0;
            for (int dz = 0; dz < 16; ++dz) for (int dx = 0; dx < 16; ++dx) mean += samples[(z + dz) * size + x + dx].g;
            means.push_back(mean / 256);
        }
        return means;
    };
    const auto uniformMeans = windowMeans(uniformRegions), regionMeans = windowMeans(shaped);
    auto deviation = [](const std::vector<double>& means) {
        double sum = 0, square = 0;
        for (double mean : means) { sum += mean; square += mean * mean; }
        return std::sqrt(std::max(0.0, square / means.size() - std::pow(sum / means.size(), 2)));
    };
    auto contrast = [&](const std::vector<double>& means) {
        double sum = 0; for (double mean : means) sum += mean;
        return deviation(means) / std::max(sum / means.size(), 0.000001);
    };
    for (std::size_t i = 0; i < regionMeans.size(); ++i)
        regionMetrics << i % 8 << ',' << i / 8 << ',' << uniformMeans[i] << ',' << regionMeans[i] << '\n';
    std::cout << "Cloud regional opacity contrast (std/mean): uniform=" << contrast(uniformMeans) << " regional=" << contrast(regionMeans)
        << " min=" << *std::min_element(regionMeans.begin(), regionMeans.end()) << " max=" << *std::max_element(regionMeans.begin(), regionMeans.end()) << '\n';
    // Normalize by mean opacity so merely reducing all density cannot pass.
    check(contrast(regionMeans) > contrast(uniformMeans) * 1.5, "Cloud density remains uniform when averaged over broad XZ regions");
    check(*std::min_element(regionMeans.begin(), regionMeans.end()) < 0.01 && *std::max_element(regionMeans.begin(), regionMeans.end()) > 0.15,
        "Regional weather must produce broad clear areas as well as visible cloud banks");
    check(bool(regionMetrics), "Cannot write regional cloud distribution metrics");

    v.applyPreset(CloudVolumeComponent::Preset::ScatteredCumulus); v.regionCoverage = 0;
    for (const auto& p : drawDensity(v)) check(p.r == 0 && p.g == 0 && p.a == 0, "Zero regional coverage retained cloud density");
    auto previousCoverage = drawDensity(v);
    for (float amount : {0.01f, 0.5f, 0.8f, 0.99f, 1.0f}) {
        v.regionCoverage = amount; const auto moreCoverage = drawDensity(v);
        for (std::size_t i = 0; i < previousCoverage.size(); ++i)
            check(moreCoverage[i].g + 0.00001f >= previousCoverage[i].g, "Increasing regional coverage removed cloud columns");
        previousCoverage = moreCoverage;
    }
    v.applyPreset(CloudVolumeComponent::Preset::ScatteredCumulus); v.densityVariation = 0;
    const auto unvaried = drawDensity(v);
    for (std::size_t i = 0; i < shaped.size(); ++i)
        check(unvaried[i].g + 0.00001f >= shaped[i].g, "Regional density modulation added extinction above the density multiplier");
    v.applyPreset(CloudVolumeComponent::Preset::ScatteredCumulus); v.windVelocity = glm::vec3(35, 0, -23);
    densityShader.setFloat("time", 17); densityShader.setVec2("worldOrigin", densityOrigin + glm::vec2(v.windVelocity.x, v.windVelocity.z) * 17.0f);
    const auto advected = drawDensity(v);
    for (int c = 0; c < 4; ++c) check(meanDifference(shaped, advected, c) < 0.0003, "Regional coverage, density or height slipped under wind advection");
    densityShader.setFloat("time", 0); densityShader.setVec2("worldOrigin", densityOrigin);
    for (float h : {0.0f, 1.0f}) {
        densityShader.setFloat("sampleHeight", h);
        for (const auto& p : drawDensity(v)) check(p.r == 0, "Cloud density is nonzero at the layer boundary");
    }
    check(bool(densityMetrics), "Cannot write cloud density metrics");
    fbo.unbind(); glUseProgram(0);
    std::cout << "PASS: actual preset density fields, clear sky columns, coverage monotonicity, erosion, density scaling and height bounds\n";
}
