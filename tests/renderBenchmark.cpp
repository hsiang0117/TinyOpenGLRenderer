#include "rendering/renderSystem.hpp"
#include "rendering/camera.hpp"
#include "assets/resourceManager.hpp"
#include "assets/shaderCatalog.hpp"
#include "scene/component.hpp"
#include "scene/scene.hpp"
#include "rendering/opengl/shader.hpp"
#include "scene/gameObjectFactory.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
int debugErrors = 0;
void APIENTRY debugCallback(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* text, const void*) {
    if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR || severity == GL_DEBUG_SEVERITY_HIGH) {
        ++debugErrors; std::cerr << "GL: " << text << '\n';
    }
}
struct Options {
    fs::path output; int width = 1280, height = 720, warmup = 60, frames = 120, rounds = 3;
    bool cloudPresets = false, cloudReference = false, cloudHorizon = false;
    bool cloudDistributionReference = false;
};
Options arguments(int argc, char** argv) {
    Options result;
    for (int i = 1; i < argc; i += 2) {
        require(i + 1 < argc, "Argument value missing"); const std::string key = argv[i];
        if (key == "--output") result.output = fs::absolute(argv[i + 1]);
        else {
            const int value = std::stoi(argv[i + 1]);
            if (key == "--width") result.width = value;
            else if (key == "--height") result.height = value;
            else if (key == "--warmup") result.warmup = value;
            else if (key == "--frames") result.frames = value;
            else if (key == "--rounds") result.rounds = value;
            else if (key == "--cloud-presets") result.cloudPresets = value != 0;
            else if (key == "--cloud-reference") result.cloudReference = value != 0;
            else if (key == "--cloud-horizon") result.cloudHorizon = value != 0;
            else if (key == "--cloud-distribution-reference") result.cloudDistributionReference = value != 0;
            else throw std::runtime_error("Unknown argument: " + key);
        }
    }
    require(!result.output.empty() && result.width >= 64 && result.width <= 3840 && result.height >= 64 && result.height <= 2160 &&
        result.frames >= 30 && result.frames <= 10000 && result.warmup >= 10 && result.warmup <= 10000 &&
        result.rounds >= 1 && result.rounds <= 20, "Invalid benchmark arguments"); return result;
}
struct IsolatedFiles {
    fs::path original = fs::current_path();
    explicit IsolatedFiles(const fs::path& output, bool cloudPresets, bool cloudReference) {
        const auto settingsName = fs::path(ShaderCatalog::settingsPath).filename();
        const auto temporaryName = fs::path(std::string(ShaderCatalog::settingsPath) + ".tmp").filename();
        fs::create_directories(output / ShaderCatalog::directory); fs::create_directories(output / "data/cache");
        for (const auto& entry : fs::directory_iterator(original / ShaderCatalog::directory)) {
            const auto name = entry.path().filename();
            if (entry.is_regular_file() && name != settingsName && name != temporaryName)
                fs::copy_file(entry.path(), output / ShaderCatalog::directory / name);
        }
        if (fs::exists(original / "data/cache")) {
            for (const auto& entry : fs::directory_iterator(original / "data/cache")) {
                if (entry.is_regular_file() && entry.path().extension() == ".noise")
                    fs::copy_file(entry.path(), output / "data/cache" / entry.path().filename());
            }
        }
        std::ofstream settings(output / ShaderCatalog::settingsPath);
        fs::create_directories(output / "data/textures/skybox");
        for (const auto& entry : fs::directory_iterator(original / "data/textures/skybox")) {
            if (entry.is_regular_file()) fs::copy_file(entry.path(), output / "data/textures/skybox" / entry.path().filename());
        }
        settings << "#define PCF_SHADOW\n#define USE_HDR\n//#define USE_BLOOM\n//#define USE_ENVIRONMENT_MAPPING\n";
        require(bool(settings), "Cannot create isolated benchmark settings");
        if (cloudPresets) {
            // Clear gradient backdrop in this isolated run only, so photographic
            // clouds in the shipped skybox cannot be mistaken for volume detail.
            std::ofstream sky(output / ShaderCatalog::skybox.fragmentPath);
            sky << "#version 450 core\nin vec3 texCoords;\n"
                "layout(location=0) out vec4 fragColor;\nlayout(location=1) out vec4 brightColor;\n"
                "void main(){float h=clamp(normalize(texCoords).y,0.0,1.0);"
                "fragColor=vec4(mix(vec3(0.24,0.46,0.7),vec3(0.035,0.15,0.4),h),1.0);"
                "brightColor=vec4(0.0,0.0,0.0,1.0);}\n";
            require(bool(sky), "Cannot write cloud preview backdrop");
        }
        if (cloudReference) {
            fs::copy_file(original / "tests/shaders/cloudRaymarchReference.frag", output / ShaderCatalog::cloudRaymarch.fragmentPath,
                fs::copy_options::overwrite_existing);
            fs::create_directories(output / "tests/shaders");
            fs::copy_file(original / "tests/shaders/cloudDensityReference.glsl", output / "tests/shaders/cloudDensityReference.glsl");
        }
        fs::current_path(output);
    }
    ~IsolatedFiles() { std::error_code error; fs::current_path(original, error); }
};
struct Scenario {
    const char* name; int clouds, points; bool bloom = false, skyLayer = false; float cameraX = 0;
    int preset = -1; float sunYaw = 20, sunElevation = -45;
    float skyPitch = 55; bool temporal = true; int viewSteps = 96, lightSteps = 8;
};
const Scenario scenarios[] = {
    {"no_cloud", 0, 0}, {"point_1", 0, 1}, {"point_4", 0, 4}, {"point_10", 0, 10},
    {"cloud_1", 1, 0}, {"cloud_4", 4, 0}, {"cloud_4_point_4", 4, 4}, {"bloom_on", 0, 0, true},
    {"sky_empty", 0, 0, false, true}, {"sky_layer", 1, 0, false, true},
    {"sky_empty_travel", 0, 0, false, true, 12000}, {"sky_travel", 1, 0, false, true, 12000},
    {"control_repeat", 0, 0}
};
ScenePtr makeScene(const Scenario& scenario, const Options& options) {
    auto scene = std::make_shared<Scene>(); scene->setName(scenario.name);
    if (scenario.preset >= 0) {
        auto sky = GameObjectFactory::createSkybox();
        sky->getComponent<SkyboxComponent>()->setSkybox("data/textures/skybox"); scene->addObject(sky);
    }
    if (!options.cloudHorizon) {
        auto ground = GameObjectFactory::createPlane();
        ground->getComponent<Transform>()->translate = glm::vec3(0, -1.5f, -12);
        ground->getComponent<Transform>()->scale = glm::vec3(80, 1, 80); scene->addObject(ground);
        for (int z = 0; z < 8; ++z) for (int x = 0; x < 8; ++x) {
            auto object = GameObjectFactory::createCube();
            object->getComponent<Transform>()->translate = glm::vec3((x - 3.5f) * 4, 0, -z * 4.0f);
            object->getComponent<Transform>()->scale = glm::vec3(1.5f, 3, 1.5f); scene->addObject(object);
        }
    }
    auto sun = GameObjectFactory::createDirectionalLight();
    sun->getComponent<Transform>()->rotate = glm::vec3(0, scenario.sunYaw, scenario.sunElevation); scene->addObject(sun);
    for (int i = 0; i < scenario.points; ++i) {
        auto light = GameObjectFactory::createPointLight();
        light->getComponent<Transform>()->translate = glm::vec3((i % 5 - 2) * 6.0f, 6, -(i / 5) * 18.0f - 5);
        scene->addObject(light);
    }
    for (int i = 0; i < scenario.clouds; ++i) {
        auto cloud = GameObjectFactory::createCloudVolume(); auto transform = cloud->getComponent<Transform>();
        const glm::vec3 offset = i == 0 ? glm::vec3(0) : glm::vec3((i & 1) ? 18 : -18, 0, (i & 2) ? -12 : 12);
        transform->translate = glm::vec3(0, 18, -30) + offset; transform->scale = glm::vec3(110, 30, 100);
        auto marker = cloud->getComponent<CloudVolumeComponent>(); marker->windVelocity = glm::vec3(0);
        marker->temporalAccumulation = !options.cloudReference && scenario.temporal;
        marker->viewMaxSteps = scenario.viewSteps; marker->lightSteps = scenario.lightSteps;
        // Keep the original finite scenarios, and separately exercise sky weather.
        marker->mode = scenario.skyLayer ? CloudVolumeComponent::Mode::SkyLayer : CloudVolumeComponent::Mode::LocalVolume;
        if (scenario.preset >= 0) marker->applyPreset(static_cast<CloudVolumeComponent::Preset>(scenario.preset));
        else { marker->cloudType = 1.0f; marker->coverageLow = 0.6f; marker->coverageHigh = 0.9f; }
        if (options.cloudDistributionReference) {
            marker->regionCoverage = 1; marker->densityVariation = marker->heightVariation = 0;
        }
        scene->addObject(cloud);
    }
    return scene;
}
double quantile(std::vector<double> values, double p) {
    require(!values.empty(), "No timing samples"); std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(std::ceil(p * values.size()) - 1);
    return values[std::min(index, values.size() - 1)];
}
void saveBitmap(const fs::path& path, const std::vector<float>& rgba, int width, int height) {
    std::ofstream out(path, std::ios::binary);
    auto u16 = [&](unsigned value) { out.put(char(value)); out.put(char(value >> 8)); };
    auto u32 = [&](unsigned value) { for (int i = 0; i < 4; ++i) out.put(char(value >> (i * 8))); };
    out.write("BM", 2); u32(54 + width * height * 4); u32(0); u32(54); u32(40);
    u32(width); u32(height); u16(1); u16(32); u32(0); u32(width * height * 4);
    u32(0); u32(0); u32(0); u32(0);
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        for (const int channel : {2, 1, 0, 3}) out.put(char(std::lround(std::clamp(rgba[i + channel], 0.0f, 1.0f) * 255)));
    }
    require(bool(out), "Cannot write scene capture");
}
void renderFrames(RenderSystem& renderer, Camera& camera, int count) {
    for (int i = 0; i < count; ++i) {
        renderer.update(0); renderer.render(camera); glFlush(); glfwPollEvents();
    }
}
void run(const Options& options, RenderSystem& renderer, ResourceManager& resources) {
    std::ofstream device("device.txt"), raw("samples.csv"), throughput("throughput.csv"), summary("summary.csv");
    device << "GL_VENDOR=" << glGetString(GL_VENDOR) << "\nGL_RENDERER=" << glGetString(GL_RENDERER)
        << "\nGL_VERSION=" << glGetString(GL_VERSION) << "\nGLSL_VERSION=" << glGetString(GL_SHADING_LANGUAGE_VERSION) << '\n';
    raw << "scenario,round,frame,pass,cpu_ms,gpu_ms\n";
    throughput << "scenario,round,frames,elapsed_ms,ms_per_frame,scene_frames_per_second\n";
    summary << "scenario,pass,samples,cpu_median_ms,cpu_p95_ms,gpu_median_ms,gpu_p95_ms\n";
    raw << std::fixed << std::setprecision(6); throughput << std::fixed << std::setprecision(6);
    summary << std::fixed << std::setprecision(6);
    require(device && raw && throughput && summary, "Cannot open benchmark outputs");
    std::cout << "GPU: " << glGetString(GL_RENDERER) << "\nPreparing deterministic volume noise...\n" << std::flush;
    renderer.ensureCloudNoise(true); const auto noiseStart = Clock::now();
    while (!renderer.cloudNoiseReady()) {
        require(Clock::now() - noiseStart < std::chrono::seconds(120), "Noise preparation timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(5)); renderer.ensureCloudNoise(true);
    }
    Camera camera(glm::vec3(0, 6, 35)); std::vector<float> baseline;
    const Scenario presetScenarios[] = {
        {"preset_empty", 0, 0, false, true, 0, 0},
        {"preset_cumulus", 1, 0, false, true, 0, 0},
        {"preset_stratocumulus", 1, 0, false, true, 0, 1},
        {"preset_overcast", 1, 0, false, true, 0, 2},
        {"preset_empty_travel", 0, 0, false, true, 12000, 0},
        {"preset_cumulus_travel", 1, 0, false, true, 12000, 0},
        {"preset_cumulus_sunward", 1, 0, false, true, 0, 0, -35, -90},
        {"preset_cumulus_away_from_sun", 1, 0, false, true, 0, 0, 35, -90}
    };
    const Scenario horizonScenarios[] = {
        {"horizon_empty", 0, 0, false, true, 0, 0, 20, -45, 8},
        {"horizon_cumulus", 1, 0, false, true, 0, 0, 20, -45, 8},
        {"horizon_cumulus_raw", 1, 0, false, true, 0, 0, 20, -45, 8, false},
        {"horizon_cumulus_high_quality", 1, 0, false, true, 0, 0, 20, -45, 8, false, 256, 24},
        {"horizon_empty_travel", 0, 0, false, true, 12000, 0, 20, -45, 8},
        {"horizon_cumulus_travel", 1, 0, false, true, 12000, 0, 20, -45, 8},
        {"horizon_empty_15", 0, 0, false, true, 0, 0, 20, -45, 15},
        {"horizon_cumulus_15", 1, 0, false, true, 0, 0, 20, -45, 15},
        {"horizon_stratocumulus_15", 1, 0, false, true, 0, 1, 20, -45, 15},
        {"horizon_overcast_15", 1, 0, false, true, 0, 2, 20, -45, 15},
        {"horizon_empty_left", 0, 0, false, true, -12000, 0, 20, -45, 8},
        {"horizon_cumulus_left", 1, 0, false, true, -12000, 0, 20, -45, 8},
        {"horizon_empty_mid", 0, 0, false, true, 4000, 0, 20, -45, 8},
        {"horizon_cumulus_mid", 1, 0, false, true, 4000, 0, 20, -45, 8},
        {"horizon_empty_overhead", 0, 0, false, true, 0, 0},
        {"horizon_cumulus_overhead", 1, 0, false, true, 0, 0},
        {"horizon_stratocumulus_overhead", 1, 0, false, true, 0, 1},
        {"horizon_overcast_overhead", 1, 0, false, true, 0, 2}
    };
    const auto* begin = options.cloudHorizon ? std::begin(horizonScenarios) : options.cloudPresets ? std::begin(presetScenarios) : std::begin(scenarios);
    const auto* end = options.cloudHorizon ? std::end(horizonScenarios) : options.cloudPresets ? std::end(presetScenarios) : std::end(scenarios);
    std::vector<float> presetBaseline;
    for (auto cursor = begin; cursor != end; ++cursor) {
        const auto& scenario = *cursor;
        camera = Camera(scenario.skyLayer ? glm::vec3(scenario.cameraX, 0, 5) : glm::vec3(0, 6, 35));
        if (scenario.skyLayer) camera.lookDelta(0, -scenario.skyPitch / DEFAULT_SENSITIVITY);
        resources.setCurrentScene(makeScene(scenario, options));
        auto composite = resources.getShader(ShaderCatalog::composite.key);
        if (composite->settingEnabled("USE_BLOOM") != scenario.bloom)
            require(composite->applySetting("USE_BLOOM", scenario.bloom), "Cannot set isolated Bloom mode");
        composite->setFloat("exposure", 1.0f);
        renderer.setProfilingEnabled(false);
        std::cout << "WARMUP " << scenario.name << '\n' << std::flush;
        const auto warmStart = Clock::now(); renderFrames(renderer, camera, options.warmup); glFinish();
        while (Clock::now() - warmStart < std::chrono::seconds(1)) { renderFrames(renderer, camera, 10); glFinish(); }
        std::vector<float> capture(options.width * options.height * 4);
        glBindTexture(GL_TEXTURE_2D, renderer.getSceneColorTextureId());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, capture.data()); glBindTexture(GL_TEXTURE_2D, 0);
        for (const auto value : capture) require(std::isfinite(value), "Scene capture contains nonfinite pixels");
        if (scenario.clouds == 0 && scenario.points == 0 && !scenario.bloom) baseline = capture;
        if (std::string(scenario.name) == "preset_empty") presetBaseline = capture;
        if (options.cloudPresets && !options.cloudHorizon && scenario.preset >= 0 && scenario.clouds > 0 && scenario.cameraX == 0) baseline = presetBaseline;
        if (scenario.clouds > 0 && scenario.points == 0) {
            std::size_t changed = 0;
            for (std::size_t i = 0; i < capture.size(); i += 4) {
                if (std::abs(capture[i] - baseline[i]) + std::abs(capture[i + 1] - baseline[i + 1]) +
                    std::abs(capture[i + 2] - baseline[i + 2]) > 0.01) ++changed;
            }
            // A broad clear-weather region is valid at the extra travel sites.
            if (!options.cloudHorizon || scenario.cameraX == 0 || scenario.cameraX == 12000)
                require(changed > std::size_t(options.width * options.height / 20), "Cloud fixture does not visibly affect enough pixels");
            std::cout << "VISIBLE " << scenario.name << " changed_pixels=" << changed << '\n' << std::flush;
        }
        saveBitmap(std::string(scenario.name) + ".bmp", capture, options.width, options.height);
        std::vector<RenderProfile> profiles;
        for (int round = 1; round <= options.rounds; ++round) {
            std::cout << "PROFILE " << scenario.name << " round=" << round << '\n' << std::flush;
            renderer.setProfilingEnabled(true); glFinish();
            for (int frame = 0; frame < 10; ++frame) { renderFrames(renderer, camera, 1); glFinish(); }
            require(renderer.getRenderProfile().valid, "GPU profiling did not produce warmup results");
            // Exclude even the last warmup frame, whose result is published one frame later.
            std::uint64_t previousFrame = renderer.getRenderProfile().frame + renderer.profileAgeFrames();
            int collected = 0;
            for (int i = 0; i <= options.frames; ++i) {
                renderFrames(renderer, camera, 1);
                // Benchmark-only drain outside RenderSystem's measured interval.
                // Per-pass CPU is submission time; pipelined throughput is measured separately.
                glFinish();
                const auto& sample = renderer.getRenderProfile();
                if (!sample.valid || sample.frame <= previousFrame) continue;
                previousFrame = sample.frame; ++collected; profiles.push_back(sample);
                auto row = [&](const char* name, double cpu, double gpu) {
                    raw << scenario.name << ',' << round << ',' << sample.frame << ',' << name << ',' << cpu << ',' << gpu << '\n';
                };
                row("Total", sample.cpuMs, sample.gpuMs); row("Prepare", sample.prepareCpuMs, sample.prepareGpuMs);
                for (const auto& pass : sample.passes) row(pass.name.c_str(), pass.cpuMs, pass.gpuMs);
            }
            require(collected == options.frames, "Incomplete profiling sample batch");
            renderer.setProfilingEnabled(false); glFinish();
            std::cout << "THROUGHPUT " << scenario.name << " round=" << round << " samples=" << collected << '\n' << std::flush;
            const auto start = Clock::now(); int throughputFrames = 0;
            // Keep a batch in flight, then drain it. This measures pipelined
            // throughput without queuing minutes of GPU work on heavier clouds.
            do {
                renderFrames(renderer, camera, options.frames); glFinish(); throughputFrames += options.frames;
            } while (Clock::now() - start < std::chrono::seconds(1));
            glFinish();
            const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            throughput << scenario.name << ',' << round << ',' << throughputFrames << ',' << elapsed << ','
                << elapsed / throughputFrames << ',' << throughputFrames * 1000.0 / elapsed << '\n';
            require(glGetError() == GL_NO_ERROR && debugErrors == 0, "OpenGL errors during benchmark");
        }
        auto statistics = [&](const char* name, auto cpu, auto gpu) {
            std::vector<double> cpuValues, gpuValues;
            for (const auto& sample : profiles) { cpuValues.push_back(cpu(sample)); gpuValues.push_back(gpu(sample)); }
            summary << scenario.name << ',' << name << ',' << profiles.size() << ',' << quantile(cpuValues, 0.5) << ','
                << quantile(cpuValues, 0.95) << ',' << quantile(gpuValues, 0.5) << ',' << quantile(gpuValues, 0.95) << '\n';
        };
        statistics("Total", [](const RenderProfile& p) { return p.cpuMs; }, [](const RenderProfile& p) { return p.gpuMs; });
        statistics("Prepare", [](const RenderProfile& p) { return p.prepareCpuMs; }, [](const RenderProfile& p) { return p.prepareGpuMs; });
        for (std::size_t i = 0; i < profiles.front().passes.size(); ++i) {
            statistics(profiles.front().passes[i].name.c_str(), [i](const RenderProfile& p) { return p.passes[i].cpuMs; },
                [i](const RenderProfile& p) { return p.passes[i].gpuMs; });
        }
        raw.flush(); throughput.flush(); summary.flush();
    }
    require(raw && throughput && summary, "Cannot finish benchmark outputs");
}
}

int main(int argc, char** argv) {
    GLFWwindow* window = nullptr;
    try {
        const auto options = arguments(argc, argv); IsolatedFiles files(options.output, options.cloudPresets || options.cloudHorizon, options.cloudReference);
        require(glfwInit() != 0, "GLFW init failed");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5); glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
        window = glfwCreateWindow(options.width, options.height, "Renderer benchmark", nullptr, nullptr);
        require(window != nullptr, "OpenGL context unavailable"); glfwMakeContextCurrent(window); glfwSwapInterval(0);
        {
            RenderSystem renderer; ResourceManager resources;
            try {
                renderer.init(); resources.init(); renderer.setResourceManager(&resources);
                renderer.setViewportSize(options.width, options.height); renderer.update(0);
                glEnable(GL_DEBUG_OUTPUT); glDebugMessageCallback(debugCallback, nullptr);
                run(options, renderer, resources);
            } catch (...) { renderer.shutdown(); resources.shutdown(); throw; }
            renderer.shutdown(); resources.shutdown();
        }
        require(glGetError() == GL_NO_ERROR && debugErrors == 0, "OpenGL errors during benchmark cleanup");
        glfwDestroyWindow(window); glfwTerminate(); std::cout << "PASS: fixed-scene Release baseline completed\n"; return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        if (window) glfwDestroyWindow(window); glfwTerminate(); return 1;
    }
}
