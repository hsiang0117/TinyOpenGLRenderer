#ifndef TOGR_RENDERSYSTEM_HPP
#define TOGR_RENDERSYSTEM_HPP
#pragma once

#include "rendering/clouds/cloudTemporal.hpp"
#include "rendering/clouds/noiseVolumeData.hpp"
#include "rendering/opengl/glBuffer.hpp"
#include "rendering/opengl/texture.hpp"
#include <glad/glad.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace togr {

class Camera;
class ResourceManager;
class SelectionModel;
class RenderPass;
struct RenderContext;

struct RenderPassTiming {
    std::string name;
    double cpuMs = 0;
    double gpuMs = 0;
};

// Latest completed scene-render sample. CPU/GPU values and metadata belong to
// the same frame; GUI drawing and swap/VSync wait are outside this measurement.
struct RenderProfile {
    bool valid = false;
    std::uint64_t frame = 0;
    int width = 0;
    int height = 0;
    std::size_t opaqueObjectCount = 0;
    std::size_t pointLightCount = 0;
    std::size_t directionalLightCount = 0;
    std::size_t spotLightCount = 0;
    std::size_t cloudVolumeCount = 0;
    double cpuMs = 0;
    double gpuMs = 0;
    double prepareCpuMs = 0;
    double prepareGpuMs = 0;
    std::vector<RenderPassTiming> passes;
};

class RenderSystem {
public:
    // Lifecycle: main thread with this window's OpenGL context current.
    // init() loads GL entry points and cleans its partial allocations on failure.
    // Call shutdown() before destroying the context, including before destruction
    // of this object; the default destructor does not coordinate context lifetime.
    RenderSystem();
    ~RenderSystem();
    void init();
    void shutdown(); // Idempotent while the GL context remains current.

    // Non-owning dependencies; their owners must outlive the frame calls.
    // ResourceManager is required by update()/render(), with shaders initialized
    // before render(); it is not needed by init()/shutdown(). Selection is optional:
    // null means no selected object and no selection gizmo.
    void setResourceManager(ResourceManager* rm) { rm_ = rm; }
    void setSelectionModel(SelectionModel* s) { selection_ = s; }

    // Frame operations: call after init(), on the main thread with current GL.
    // update() resizes targets and advances playing animations; render() snapshots
    // scene views, uploads camera data and executes the ordered passes.
    void update(double deltaTime);
    void render(Camera& camera);

    // Called by the cloud pass. First request launches CPU computation/cache I/O
    // and returns. Later calls poll all three results without waiting, then upload
    // on this GL thread. No volume means no polling; it does not cancel the jobs.
    void ensureCloudNoise(bool sceneHasVolume);

    // Reports a requested size (clamped to at least 1); update() applies it.
    // Engine calls GUI after scene rendering, so panel changes apply next frame.
    void setViewportSize(int w, int h);

    // A changed setting clears the published sample, pending samples and skipped
    // count. GPU timestamp support is checked lazily during render(); unavailable
    // timestamps turn profiling back off. A busy query ring skips new samples.
    void setProfilingEnabled(bool enabled);

    // Queries: main-thread views of current renderer state.
    // The texture ID is borrowed, not owned; zero before init()/after shutdown().
    unsigned int getSceneColorTextureId() const;
    bool cloudNoiseReady() const { return noiseReady; }
    bool profilingEnabled() const { return profilingEnabled_; }

    // Borrowed storage, not a fixed snapshot: render(), profiling changes and
    // shutdown() may replace its contents. Check valid; copy to retain a sample.
    const RenderProfile& getRenderProfile() const { return profile_; }
    std::uint64_t profileAgeFrames() const { return profile_.valid ? renderFrame_ - profile_.frame : 0; }
    std::uint64_t skippedProfileFrames() const { return skippedProfileFrames_; }

private:
    struct ProfileFrame;

    // CPU-only asynchronous results. No worker borrows renderer or GL resources.
    struct CloudNoiseJobs {
        std::future<NoiseVolumeData> worley;
        std::future<NoiseVolumeData> perlin;
        std::future<NoiseVolumeData> weather;
    };

    // Initialization stages preserve allocation order and share init()'s cleanup.
    void configureGlState();
    void initSharedBuffers();
    void initShadowTargets();
    void initViewportTargets();
    void createPasses();

    void resizeViewportTargets();
    void updateAnimations(double deltaTime);

    // Lazy noise stages; called from the cloud pass without changing its timing
    // boundary. Polling never waits; publication uploads on the current GL thread.
    void startCloudNoiseJobs();
    bool cloudNoiseJobsReady() const;
    void publishCloudNoiseTextures();
    void discardCloudNoiseJobs(); // Waits and discards CPU results without get()/GL.

    // Context is local to one synchronous render() call. Scene/shader snapshots
    // hold shared owners; target pointers borrow this renderer's member resources.
    RenderContext buildContext(Camera& camera);
    void resolveShaders(RenderContext& ctx);
    void snapshotScene(RenderContext& ctx);
    void assignTargets(RenderContext& ctx);
    RenderContext prepareFrame(Camera& camera);

    // Profiling starts after slot acquisition and includes preparation plus passes.
    // Metadata submission runs after the final CPU timestamp, outside the sample.
    ProfileFrame* acquireProfileFrame();
    void executePasses(RenderContext& ctx, ProfileFrame* capture);
    void submitProfileFrame(ProfileFrame& capture, const RenderContext& ctx, double cpuMs);

    // Asynchronous profiling: publish only completed, matching CPU/GPU samples.
    bool initProfileQueries();
    void collectProfileResults();
    void shutdownProfileQueries();
    double gpuDurationMs(GLuint64 begin, GLuint64 end) const;

    // Owned GL resources. Preserve declaration order so member destruction does
    // not change independently of the explicit shutdown() sequence.
    float width;
    float height;
    UniformBuffer cameraMatricesUbo;
    ShaderStorageBuffer pointLightsSsbo;
    ShaderStorageBuffer directionalLightSsbo;
    ShaderStorageBuffer spotLightsSsbo;
    ShaderStorageBuffer cloudVolumesSsbo;
    FrameBuffer directionalShadowFbo;
    FrameBuffer pointShadowFbo;
    FrameBuffer hdrFbo;
    FrameBuffer bloomBlurFbos[2];
    FrameBuffer overlayFbo;
    FrameBuffer cloudRaymarchFbo;
    Texture2D directionalShadowTexture;
    Texture2D hdrColorTexture;
    Texture2D bloomBrightTexture;
    Texture2D bloomBlurTextures[2];
    Texture2D overlayColorTexture;
    Texture2D hdrDepthTexture;
    Texture2D cloudRaymarchTexture;
    Texture2D weatherMapTexture;
    CloudTemporal cloudTemporal;
    Texture3D worleyNoiseTexture;
    Texture3D perlinNoiseTexture;
    CubeMapArray pointShadowTexture;

    // Composite output sampled by the Viewport panel through ImGui::Image.
    FrameBuffer sceneFbo;
    Texture2D sceneColorTexture;
    // Requested size; width/height above are the size applied by update().
    int viewportW_ = 1280;
    int viewportH_ = 720;

    // CPU request state differs from GPU readiness. Requested is set before
    // launching; a launch failure may leave only some workers started. Ready means
    // all three textures were uploaded on the main thread. shutdown() waits and
    // discards CPU results.
    bool noiseRequested = false;
    bool noiseReady = false;
    CloudNoiseJobs cloudNoiseJobs;

    // Borrowed frame dependencies (see setter contracts above).
    ResourceManager* rm_ = nullptr;
    SelectionModel* selection_ = nullptr;

    // Ordered once in init(); producer-consumer order is part of the frame contract.
    std::vector<std::unique_ptr<RenderPass>> passes;

    // Bounded query ring. An occupied slot is never overwritten before completion.
    struct ProfileFrame {
        std::vector<GLuint> queries;
        std::vector<GLuint64> timestamps;
        RenderProfile sample;
    };
    std::array<ProfileFrame, 4> profileFrames_;
    RenderProfile profile_;
    bool profilingEnabled_ = false;
    std::size_t profileRead_ = 0;
    std::size_t profileWrite_ = 0;
    std::size_t pendingProfiles_ = 0;
    std::uint64_t renderFrame_ = 0;
    std::uint64_t skippedProfileFrames_ = 0;
    GLint timestampBits_ = 0;
};

} // namespace togr

#endif // !TOGR_RENDERSYSTEM_HPP
