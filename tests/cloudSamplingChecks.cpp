#include "rendering/clouds/cloudTemporal.hpp"
#include "rendering/clouds/noiseTextureGenerator.hpp"
#include "rendering/opengl/shader.hpp"
#include "rendering/renderUtils.hpp"
#include "scene/component.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace togr::tests {

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
using Image = std::vector<glm::vec4>;
constexpr int size = 64;
CloudVolumeGpu gpu(const CloudVolumeComponent& v) {
    return {glm::vec3(-300, 100, -300), v.cloudType, glm::vec3(300, 300, 300), v.coverageLow,
        v.coverageHigh, v.baseNoiseScale, v.warpScale, v.warpStrength,
        v.detailScale, v.detailErosionStrength, v.extinctionCoefficient, v.densityMultiplier,
        v.scatteringAlbedo, v.multipleScattering, v.edgeFadeDistance, v.viewStepWorldSize, v.windVelocity,
        v.mode == CloudVolumeComponent::Mode::SkyLayer ? 1 : 0,
        v.layerBottom, v.layerThickness, v.maxTraceDistance, v.distanceFadeStart,
        v.weatherScale, v.ambientStrength, v.phaseForwardG, v.sunStrength,
        v.viewMaxSteps, v.lightSteps, v.emptyStepMultiplier, 0,
        v.regionScale, v.regionCoverage, v.densityVariation, v.heightVariation};
}
Image read(Texture2D& texture) {
    Image image(size * size); texture.use(GL_TEXTURE0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, image.data());
    for (const auto& p : image) for (int c = 0; c < 4; ++c) check(std::isfinite(p[c]), "Cloud sampling produced nonfinite pixels");
    return image;
}
double rms(const Image& a, const Image& b) {
    double sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) for (int c = 0; c < 3; ++c) sum += std::pow(double(a[i][c] - b[i][c]), 2);
    return std::sqrt(sum / (a.size() * 3));
}
CloudTemporal::Frame frame(glm::vec3 position = glm::vec3(0)) {
    CloudTemporal::Frame f;
    f.cameraPos = position; f.cameraFront = glm::normalize(glm::vec3(0, 0.8, -0.6));
    f.projection = glm::perspective(glm::radians(55.0f), 1.0f, 0.1f, 100.0f);
    f.vp = f.projection * glm::lookAt(position, position + f.cameraFront, glm::vec3(0, 1, 0));
    f.invVP = glm::inverse(f.vp); f.seconds = 17.0f;
    f.scene = reinterpret_cast<const void*>(1); f.volumes.push_back(gpu(CloudVolumeComponent()));
    f.volumes[0].windVelocity = glm::vec3(0);
    return f;
}

void samplingChecks() {
    auto perlin = NoiseTextureGenerator3D::uploadVolume3D(NoiseTextureGenerator3D::computePerlinNoise(128, 128, 128));
    auto worley = NoiseTextureGenerator3D::uploadVolume3D(NoiseTextureGenerator3D::computeWorleyNoise(128, 128, 128));
    Texture2D weather(1, 1, GL_REPEAT, GL_NEAREST, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    const unsigned char white = 255; weather.subImage2D(0, 0, 1, 1, &white);
    Texture2D opaque(size, size, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_R32F, GL_RED, GL_FLOAT);
    std::vector<float> sky(size * size, 1); opaque.subImage2D(0, 0, size, size, sky.data());
    Texture2D output(size, size, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    FrameBuffer fbo; fbo.init(); fbo.bind(); fbo.attachTexture2D(output, GL_COLOR_ATTACHMENT0);
    fbo.drawBuffers({GL_COLOR_ATTACHMENT0});
    ShaderStorageBuffer medium; medium.init();
    auto upload = [&](const CloudVolumeGpu& v) {
        medium.bind(); medium.bufferBase(4); medium.bufferData(sizeof(v), &v); medium.unbind();
    };
    Shader probe("data/shader/fullscreen.vert", "tests/shaders/cloudSamplingProbe.frag");
    check(probe.isValid(), "Cloud bounds probe failed to compile");
    auto volume = gpu(CloudVolumeComponent()); volume.windVelocity = glm::vec3(35, 0, -23);
    int certified = 0;
    for (int preset = 0; preset < 3; ++preset) for (int variant = 0; variant < 3; ++variant)
        for (auto origin : {glm::vec3(-3100.25f, 100, -2400.5f),
            glm::vec3(12000.0f, 138, -12000.0f), glm::vec3(-1200000.25f, 290, 1200000.5f)}) {
        CloudVolumeComponent v; v.applyPreset(static_cast<CloudVolumeComponent::Preset>(preset));
        if (variant != 0) v.heightVariation = 0.8f;
        if (variant == 2) v.regionCoverage = 1.0f;
        volume = gpu(v); volume.windVelocity = glm::vec3(35, 0, -23); upload(volume);
        for (const auto delta : {glm::vec3(0.1f, 0, 0.2f), glm::vec3(-7, 12, 11), glm::vec3(120, -140, -90)}) {
            fbo.bind(); glViewport(0, 0, size, size); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
            perlin.use(GL_TEXTURE0); worley.use(GL_TEXTURE1); weather.use(GL_TEXTURE2); probe.use();
            probe.setInt("perlinNoiseTexture", 0); probe.setInt("worleyNoiseTexture", 1); probe.setInt("weatherMap", 2);
            probe.setFloat("time", 17); probe.setVec3("origin", origin); probe.setVec3("blockDelta", delta);
            for (int mode = 0; mode < 2; ++mode) {
                probe.setInt("outputMode", mode); RenderHelpers::drawFullscreenQuad();
                // read() changes texture unit 0; restore the noise binding next draw.
                const auto samples = read(output); perlin.use(GL_TEXTURE0);
                for (const auto& p : samples) {
                    if (mode == 0) check(p.r <= 0.00001f && p.g <= 0.00001f && p.b >= 0 && p.a <= 1 && p.b <= p.a,
                        "Conservative weather bounds excluded an actual noise sample");
                    else { check(p.r == 0, "Certified empty block contained cloud density"); certified += p.g > 0 ? 1 : 0; }
                }
            }
        }
    }
    check(certified > size * size, "Empty-space certificate was never exercised");
    std::cout << "PASS: conservative weather ranges and empty blocks across cells, heights, wind and distant coordinates\n";

    Shader current("data/shader/cloudRaymarch.vert", "data/shader/cloudRaymarch.frag");
    Shader reference("data/shader/cloudRaymarch.vert", "tests/shaders/cloudRaymarchReference.frag");
    check(current.isValid() && reference.isValid(), "Cloud image reference failed to compile");
    std::ofstream metrics("x64/RenderChecks/cloud-sampling-differences.csv");
    metrics << "reference,preset,camera_x,camera_y,skip_multiplier,rgb_rms,max_rgba_error\n";
    auto draw = [&](Shader& shader, const CloudVolumeGpu& v, const CloudTemporal::Frame& f) {
        upload(v); fbo.bind(); glViewport(0, 0, size, size); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        opaque.use(GL_TEXTURE0); worley.use(GL_TEXTURE1); weather.use(GL_TEXTURE2); perlin.use(GL_TEXTURE3);
        shader.use(); shader.setInt("depthMap", 0); shader.setInt("worleyNoiseTexture", 1);
        shader.setInt("weatherMap", 2); shader.setInt("perlinNoiseTexture", 3); shader.setInt("cloudVolumeCount", 1);
        shader.setFloat("time", 17); shader.setFloat("jitterOffset", 0);
        shader.setVec2("resolution", glm::vec2(size)); shader.setMat4("invVP", f.invVP);
        shader.setVec3("cameraPos", f.cameraPos); shader.setVec3("lightDir", glm::normalize(glm::vec3(0.57, 0.82, 0.28)));
        shader.setVec3("lightColor", glm::vec3(1)); RenderHelpers::drawFullscreenQuad(); return read(output);
    };
    for (int preset = 0; preset < 3; ++preset) for (auto position : {glm::vec3(0), glm::vec3(12000, 0, 0), glm::vec3(-500, 150, 700)}) {
        CloudVolumeComponent v; v.applyPreset(static_cast<CloudVolumeComponent::Preset>(preset));
        v.windVelocity = glm::vec3(35, 0, -23); volume = gpu(v); const auto f = frame(position);
        volume.regionCoverage = 1; volume.densityVariation = volume.heightVariation = 0;
        const auto original = draw(reference, volume, f);
        for (int skip : {1, 4, 8}) {
            volume.emptyStepMultiplier = skip; const auto optimized = draw(current, volume, f);
            float maximum = 0;
            for (std::size_t i = 0; i < original.size(); ++i) for (int c = 0; c < 4; ++c)
                maximum = std::max(maximum, std::abs(original[i][c] - optimized[i][c]));
            metrics << "step_three," << preset << ',' << position.x << ',' << position.y << ',' << skip << ',' << rms(original, optimized) << ',' << maximum << '\n';
            check(maximum < 0.0005f, "Adaptive cloud marching changed the approved density, color or silhouette");
        }
    }
    std::cout << "PASS: optimized cloud images match the step-three reference at default sampling across all presets\n";
    // New regional shape has a different silhouette; compare its skip modes
    // against its own complete fine grid, including almost horizontal rays.
    for (int preset = 0; preset < 3; ++preset) for (auto position : {glm::vec3(0), glm::vec3(12000, 0, 0), glm::vec3(-500, 150, 700)})
        for (float pitch : {8.0f, 55.0f}) {
            CloudVolumeComponent v; v.applyPreset(static_cast<CloudVolumeComponent::Preset>(preset));
            volume = gpu(v); auto f = frame(position);
            f.cameraFront = glm::vec3(0, std::sin(glm::radians(pitch)), -std::cos(glm::radians(pitch)));
            f.vp = f.projection * glm::lookAt(position, position + f.cameraFront, glm::vec3(0, 1, 0)); f.invVP = glm::inverse(f.vp);
            volume.emptyStepMultiplier = 1; const auto fine = draw(current, volume, f);
            for (int skip : {4, 8}) {
                volume.emptyStepMultiplier = skip; const auto skipped = draw(current, volume, f);
                float maximum = 0;
                for (std::size_t i = 0; i < fine.size(); ++i) for (int c = 0; c < 4; ++c)
                    maximum = std::max(maximum, std::abs(fine[i][c] - skipped[i][c]));
                metrics << "regional_" << pitch << ',' << preset << ',' << position.x << ',' << position.y << ',' << skip << ',' << rms(fine, skipped) << ',' << maximum << '\n';
                check(maximum < 0.0005f, "Empty-space skipping lost regional clouds at low or high view angles");
            }
        }
    check(bool(metrics), "Cannot write cloud sampling image comparisons"); fbo.unbind(); glUseProgram(0);
    std::cout << "PASS: regional density matches its full fine grid at low angles, overhead and inside the layer\n";
}

void temporalChecks() {
    CloudTemporal temporal; temporal.init(size, size);
    Texture2D raw(size, size, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_RGBA32F, GL_RGBA, GL_FLOAT);
    FrameBuffer fbo; fbo.init(); fbo.bind(); fbo.attachTexture2D(raw, GL_COLOR_ATTACHMENT0);
    fbo.attachTexture2D(temporal.currentDepth(), GL_COLOR_ATTACHMENT1);
    fbo.drawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1}); fbo.unbind();
    Shader input("data/shader/fullscreen.vert", "tests/shaders/cloudTemporalInput.frag");
    Shader resolve("data/shader/fullscreen.vert", "data/shader/cloudTemporal.frag");
    check(input.isValid() && resolve.isValid(), "Temporal cloud test failed to compile");
    auto f = frame(); f.raymarchProgram = input.ID; f.resolveProgram = resolve.ID;
    auto draw = [&](float noise, int phase, bool clear = false, bool foreground = false) {
        fbo.bind(); glViewport(0, 0, size, size); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); input.use();
        input.setMat4("invVP", f.invVP); input.setVec3("cameraPos", f.cameraPos);
        input.setVec3("velocity", f.volumes[0].windVelocity); input.setFloat("seconds", f.seconds);
        input.setFloat("noiseAmplitude", noise); input.setInt("noisePhase", phase);
        input.setInt("clearCloud", clear ? 1 : 0); input.setInt("foreground", foreground ? 1 : 0);
        input.setVec2("resolution", glm::vec2(size)); RenderHelpers::drawFullscreenQuad(); fbo.unbind();
    };
    auto resolved = [&] { return read(temporal.resolve(raw, resolve)); };
    temporal.beginFrame(f, true); check(temporal.active() && !temporal.historyUsable(), "Cold cloud history was considered valid");
    draw(0, 0); const auto clean = read(raw); const auto first = resolved();
    check(rms(clean, first) < 0.0005, "Cold temporal frame changed the current cloud image");
    double rawError = 0, historyError = 0;
    for (int i = 0; i < 20; ++i) {
        f.seconds += 0.016f; temporal.beginFrame(f, true);
        check(temporal.historyUsable() && temporal.jitterOffset() >= 0 && temporal.jitterOffset() < 1,
            "Static history did not persist or jitter was invalid");
        draw(0.04f, i); const auto noisy = read(raw); const auto filtered = resolved();
        rawError = rms(noisy, clean); historyError = rms(filtered, clean);
        for (const auto& p : filtered) check(p.a > 0.599f && p.a < 0.601f, "Temporal filtering changed homogeneous cloud opacity");
    }
    std::cout << "Cloud temporal noise RMS: raw=" << rawError << " resolved=" << historyError << '\n';
    check(historyError < rawError * 0.3, "Temporal reprojection did not reduce sampling noise");

    const auto nearFrame = f;
    f = frame(glm::vec3(12000, 0, -12000)); f.raymarchProgram = input.ID; f.resolveProgram = resolve.ID;
    temporal.invalidate(); temporal.beginFrame(f, true); draw(0, 0); const auto distant = read(raw); resolved();
    for (int i = 0; i < 16; ++i) {
        f.seconds += 0.016f; temporal.beginFrame(f, true); draw(0, 0);
        check(rms(resolved(), distant) < 0.0003, "Stationary cloud history drifted at distant world coordinates");
    }
    f = nearFrame;

    // Stabilize a clean frame, then translate the camera and advect the cloud.
    temporal.invalidate(); temporal.beginFrame(f, true); draw(0, 0); resolved();
    f.cameraPos.x += 20; f.vp = f.projection * glm::lookAt(f.cameraPos, f.cameraPos + f.cameraFront, glm::vec3(0, 1, 0));
    f.invVP = glm::inverse(f.vp); f.seconds += 0.1f; temporal.beginFrame(f, true);
    check(temporal.historyUsable(), "Ordinary camera translation discarded history");
    draw(0, 0); const auto moved = read(raw); const auto movedHistory = resolved();
    check(rms(movedHistory, moved) < 0.0015, "Cloud history was reprojected in screen space instead of world space");
    f.volumes[0].windVelocity = glm::vec3(45, 0, -25); temporal.beginFrame(f, true);
    check(!temporal.historyUsable(), "Wind edit did not reset cloud history"); draw(0, 0); resolved();
    f.seconds += 0.1f; temporal.beginFrame(f, true); draw(0, 0);
    const auto windy = read(raw); const auto windHistory = resolved();
    check(temporal.historyUsable() && rms(windHistory, windy) < 0.0015, "Cloud reprojection did not compensate for wind advection");

    f.seconds += 0.016f; temporal.beginFrame(f, true); draw(0, 0, false, true); const auto blocked = resolved();
    for (int y = 0; y < size; ++y) for (int x = 0; x < size / 2; ++x)
        check(blocked[y * size + x] == glm::vec4(0), "Foreground left cloud history over opaque geometry");
    f.seconds += 0.016f; temporal.beginFrame(f, true); draw(0, 0); const auto exposed = read(raw); const auto disoccluded = resolved();
    check(rms(exposed, disoccluded) < 0.0015, "Cloud disocclusion kept a foreground history trail");
    f.seconds += 0.016f; temporal.beginFrame(f, true); draw(0, 0, true); const auto empty = resolved();
    for (const auto& p : empty) check(p == glm::vec4(0), "Cloud history survived a clear current pixel");
    std::cout << "PASS: temporal noise reduction, distant stationary view, camera reprojection, wind, opaque disocclusion and clear-sky rejection\n";

    auto warm = [&] { temporal.beginFrame(f, true); draw(0, 0); resolved(); };
    auto reset = [&](const CloudTemporal::Frame& changed, const char* message) {
        warm(); temporal.beginFrame(changed, true); check(!temporal.historyUsable() && temporal.jitterOffset() == 0, message);
    };
    auto changed = f; changed.volumes[0].densityMultiplier += 0.1f; reset(changed, "Density edit retained stale cloud history");
    changed = f; changed.volumes[0].regionCoverage -= 0.1f; reset(changed, "Regional weather edit retained stale cloud history");
    changed = f; ++changed.volumes[0].viewMaxSteps; reset(changed, "Sampling edit retained stale cloud history");
    changed = f; changed.lightColor.r += 0.1f; reset(changed, "Light edit retained stale cloud history");
    changed = f; changed.scene = reinterpret_cast<const void*>(2); reset(changed, "Scene switch retained stale cloud history");
    changed = f; ++changed.raymarchProgram; reset(changed, "Raymarch shader reload retained stale cloud history");
    changed = f; ++changed.resolveProgram; reset(changed, "Resolve shader reload retained stale cloud history");
    changed = f; ++changed.noiseTextures[0]; reset(changed, "Noise replacement retained stale cloud history");
    changed = f; changed.projection[0][0] *= 1.1f; reset(changed, "FOV change retained stale cloud history");
    changed = f; changed.cameraPos.x += 12000; reset(changed, "Camera teleport retained stale cloud history");
    changed = f; changed.cameraFront = -f.cameraFront; reset(changed, "Camera cut retained stale cloud history");
    changed = f; changed.seconds += 1; reset(changed, "Long frame interruption retained stale cloud history");
    changed = f; changed.seconds -= 1; reset(changed, "Time reversal retained stale cloud history");
    warm(); temporal.beginFrame(f, false); check(!temporal.active(), "Temporal toggle did not disable history");
    changed = f; changed.volumes[0].skyLayer = 0; temporal.beginFrame(changed, true);
    check(!temporal.active(), "Local volume incorrectly used the sky motion model");
    changed = f; changed.volumes[0].windVelocity.y = 1; temporal.beginFrame(changed, true);
    check(!temporal.active(), "Vertical noise motion incorrectly advected the fixed height profile");
    changed = f; changed.volumes.push_back(changed.volumes[0]); changed.volumes[1].windVelocity.x += 1;
    temporal.beginFrame(changed, true); check(!temporal.active(), "Different cloud winds incorrectly shared one motion model");
    changed = f; changed.volumes.clear(); temporal.beginFrame(changed, true); check(!temporal.active(), "Empty scene retained cloud history");
    warm(); const auto depthId = temporal.currentDepth().ID; temporal.resize(size + 1, size + 1);
    check(temporal.currentDepth().ID == depthId, "Cloud resize broke the raw framebuffer attachment");
    temporal.beginFrame(f, true); check(!temporal.historyUsable(), "Viewport resize retained cloud history");
    temporal.destroy(); temporal.destroy(); check(!glIsTexture(depthId), "Cloud history metadata leaked after shutdown");
    glUseProgram(0);
    std::cout << "PASS: cloud history invalidation on edits, cuts, scene/shader/noise changes, resize and unsupported motion\n";
}
}

void runCloudSamplingChecks() {
    std::filesystem::create_directories("x64/RenderChecks");
    samplingChecks(); temporalChecks();
}

} // namespace togr::tests
