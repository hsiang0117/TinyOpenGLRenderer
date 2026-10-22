#include "renderSystem.hpp"
#include "scene/selectionModel.hpp"
#include "assets/resourceManager.hpp"
#include "assets/shaderCatalog.hpp"
#include "scene/component.hpp"
#include "renderLimits.hpp"
#include "renderUtils.hpp"
#include "rendering/passes/renderPass.hpp"
#include "rendering/passes/renderPasses.hpp"
#include "core/logger.hpp"
#include "rendering/clouds/noiseTextureGenerator.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <stdexcept>
#include <chrono>

namespace togr {

RenderSystem::RenderSystem() = default;
RenderSystem::~RenderSystem() = default;

static void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
    GLsizei, const char* message, const void*) {
    // Skip low-value notifications (e.g. buffer detail messages)
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    const char* sourceStr = "Other";
    switch (source) {
        case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     sourceStr = "Application"; break;
    }

    const char* typeStr = "Other";
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "Undefined Behavior"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              typeStr = "Marker"; break;
    }

    const char* severityStr = "Low";
    LogLevel level = LogLevel::Info;
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:   severityStr = "High";   level = LogLevel::Error;   break;
        case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "Medium"; level = LogLevel::Warning; break;
    }

    TOGR_LOG_AT(level, "[GL][" << severityStr << "][" << sourceStr << "][" << typeStr
        << "] (" << id << ") " << message);
}

void RenderSystem::init() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        TOGR_LOG_ERROR("Failed to initialize GLAD");
        throw std::runtime_error("Failed to load OpenGL functions (OpenGL 4.5 is required)");
    }
    try {
        configureGlState();
        initSharedBuffers();
        initShadowTargets();
        initViewportTargets();
        createPasses();
    } catch (...) {
        // Initialization may stop after any allocation; release it before the
        // caller tears down the window and its current GL context.
        shutdown();
        throw;
    }
}

void RenderSystem::configureGlState() {
#ifdef _DEBUG
    {
        GLint flags = 0;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glDebugOutput, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }
    }
#endif
    // Size-dependent targets follow the Viewport panel, not the window. Seed from
    // the initial viewport guess; update() resizes once the panel reports its size.
    width = (float)viewportW_;
    height = (float)viewportH_;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDepthFunc(GL_LEQUAL);
}

void RenderSystem::initSharedBuffers() {
    cameraMatricesUbo.init();
    pointLightsSsbo.init();
    directionalLightSsbo.init();
    spotLightsSsbo.init();
    cloudVolumesSsbo.init();

    cameraMatricesUbo.bind();
    cameraMatricesUbo.bufferBase(0);
    cameraMatricesUbo.bufferData(2 * sizeof(glm::mat4), NULL);
    cameraMatricesUbo.unbind();

    pointLightsSsbo.bind();
    pointLightsSsbo.bufferBase(1);
    pointLightsSsbo.bufferData(RenderLimits::PointLights * PointLightComponent::glslSize, NULL);
    pointLightsSsbo.unbind();

    directionalLightSsbo.bind();
    directionalLightSsbo.bufferBase(2);
    directionalLightSsbo.bufferData(DirectionalLightComponent::glslSize, NULL);
    directionalLightSsbo.unbind();

    spotLightsSsbo.bind();
    spotLightsSsbo.bufferBase(3);
    spotLightsSsbo.bufferData(RenderLimits::SpotLights * SpotLightComponent::glslSize, NULL);
    spotLightsSsbo.unbind();
}

void RenderSystem::initShadowTargets() {
    directionalShadowTexture = Texture2D(1024, 1024, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
    directionalShadowTexture.setBorderColor(1.0, 1.0, 1.0, 1.0);
    directionalShadowFbo.init();
    directionalShadowFbo.bind();
    directionalShadowFbo.attachTexture2D(directionalShadowTexture, GL_DEPTH_ATTACHMENT);
    directionalShadowFbo.drawBuffers({ GL_NONE });
    directionalShadowFbo.readBuffer(GL_NONE);
    directionalShadowFbo.requireComplete("Directional shadow framebuffer");
    directionalShadowFbo.unbind();

    pointShadowTexture = CubeMapArray(1024, 1024, static_cast<int>(RenderLimits::PointLights), GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT);
    pointShadowFbo.init();
    pointShadowFbo.bind();
    pointShadowFbo.attachTexture(pointShadowTexture, GL_DEPTH_ATTACHMENT);
    pointShadowFbo.drawBuffers({ GL_NONE });
    pointShadowFbo.readBuffer(GL_NONE);
    pointShadowFbo.requireComplete("Point shadow framebuffer");
    pointShadowFbo.unbind();
}

void RenderSystem::initViewportTargets() {
    hdrFbo.init();
    hdrColorTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    bloomBrightTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    hdrDepthTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_NEAREST, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
    // Border = far plane (1.0): if anything ever samples depth outside [0,1] (e.g. a
    // future temporal-reprojection tap), it reads "no geometry" rather than near 0.0.
    hdrDepthTexture.setBorderColor(1.0, 1.0, 1.0, 1.0);
    hdrFbo.bind();
    hdrFbo.attachTexture2D(hdrColorTexture, GL_COLOR_ATTACHMENT0);
    hdrFbo.attachTexture2D(bloomBrightTexture, GL_COLOR_ATTACHMENT1);
    hdrFbo.attachTexture2D(hdrDepthTexture, GL_DEPTH_ATTACHMENT);
    hdrFbo.drawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
    hdrFbo.requireComplete("HDR framebuffer");
    hdrFbo.unbind();

    bloomBlurFbos[0].init();
    bloomBlurFbos[1].init();
    bloomBlurTextures[0] = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    bloomBlurTextures[1] = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    bloomBlurFbos[0].bind();
    bloomBlurFbos[0].attachTexture2D(bloomBlurTextures[0], GL_COLOR_ATTACHMENT0);
    bloomBlurFbos[0].requireComplete("Bloom blur framebuffer 0");
    bloomBlurFbos[0].unbind();
    bloomBlurFbos[1].bind();
    bloomBlurFbos[1].attachTexture2D(bloomBlurTextures[1], GL_COLOR_ATTACHMENT0);
    bloomBlurFbos[1].requireComplete("Bloom blur framebuffer 1");
    bloomBlurFbos[1].unbind();

    overlayFbo.init();
    overlayColorTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_BORDER, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    overlayFbo.bind();
    overlayFbo.attachTexture2D(overlayColorTexture, GL_COLOR_ATTACHMENT0);
    overlayFbo.requireComplete("Overlay framebuffer");
    overlayFbo.unbind();

    // Half-resolution cloud raymarch target. Clouds are low frequency, so the
    // expensive march runs at half the viewport size (~4x fewer fragments); the
    // CloudSkeletonOverlayPass then depth-aware-upsamples it into overlayFbo.
    // CLAMP_TO_EDGE + LINEAR: the upsample samples it bilinearly and must not pull
    // border texels at the edges.
    cloudRaymarchFbo.init();
    cloudRaymarchTexture = Texture2D((viewportW_ + 1) / 2, (viewportH_ + 1) / 2, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    cloudTemporal.init((viewportW_ + 1) / 2, (viewportH_ + 1) / 2);
    cloudRaymarchFbo.bind();
    cloudRaymarchFbo.attachTexture2D(cloudRaymarchTexture, GL_COLOR_ATTACHMENT0);
    cloudRaymarchFbo.attachTexture2D(cloudTemporal.currentDepth(), GL_COLOR_ATTACHMENT1);
    cloudRaymarchFbo.drawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
    cloudRaymarchFbo.requireComplete("Cloud raymarch framebuffer");
    cloudRaymarchFbo.unbind();

    // Offscreen scene target for the Viewport panel. Color-only (no depth needed for
    // the composite fullscreen quad), CLAMP_TO_EDGE + LINEAR so ImGui samples it clean.
    sceneFbo.init();
    sceneColorTexture = Texture2D(viewportW_, viewportH_, GL_CLAMP_TO_EDGE, GL_LINEAR, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    sceneFbo.bind();
    sceneFbo.attachTexture2D(sceneColorTexture, GL_COLOR_ATTACHMENT0);
    sceneFbo.drawBuffers({ GL_COLOR_ATTACHMENT0 });
    sceneFbo.requireComplete("Scene framebuffer");
    sceneFbo.unbind();
}

void RenderSystem::createPasses() {
    // Register the fixed producer-consumer order. LightUpload supplies data for
    // ForwardHdr; the forward pass establishes its own input bindings.
    passes.clear();
    passes.push_back(std::make_unique<FrameSetupPass>());
    passes.push_back(std::make_unique<DirectionalShadowPass>());
    passes.push_back(std::make_unique<PointShadowPass>());
    passes.push_back(std::make_unique<LightUploadPass>());
    passes.push_back(std::make_unique<ForwardHdrPass>());
    passes.push_back(std::make_unique<CloudSkeletonOverlayPass>());
    passes.push_back(std::make_unique<GizmoPass>());
    passes.push_back(std::make_unique<BloomPass>());
    passes.push_back(std::make_unique<CompositePass>());
}

void RenderSystem::shutdown() {
    shutdownProfileQueries();
    discardCloudNoiseJobs();
    noiseReady = false;
    passes.clear();

    for (auto* fbo : { &directionalShadowFbo, &pointShadowFbo, &hdrFbo,
        &bloomBlurFbos[0], &bloomBlurFbos[1], &overlayFbo, &cloudRaymarchFbo, &sceneFbo }) {
        fbo->destroy();
    }
    cloudTemporal.destroy();
    Texture* textures[] = { &directionalShadowTexture, &hdrColorTexture,
        &bloomBrightTexture, &bloomBlurTextures[0], &bloomBlurTextures[1], &overlayColorTexture,
        &hdrDepthTexture, &cloudRaymarchTexture, &weatherMapTexture, &sceneColorTexture,
        &worleyNoiseTexture, &perlinNoiseTexture, &pointShadowTexture };
    for (auto* texture : textures) {
        texture->destroy();
    }
    cameraMatricesUbo.destroy();
    pointLightsSsbo.destroy();
    directionalLightSsbo.destroy();
    spotLightsSsbo.destroy();
    cloudVolumesSsbo.destroy();
    RenderHelpers::shutdownSharedGeometry();
    destroyDummyTexture2D();
    destroyDummyCubeMap();
}

void RenderSystem::ensureCloudNoise(bool sceneHasVolume) {
    if (noiseReady) return;
    if (!sceneHasVolume) return;

    if (!noiseRequested) {
        startCloudNoiseJobs();
        return;
    }

    if (cloudNoiseJobsReady()) {
        publishCloudNoiseTextures();
    }
}

void RenderSystem::startCloudNoiseJobs() {
    // Record the request before launching. A partial launch failure still leaves
    // already-started jobs owned here for shutdown(); this is not a retry state.
    noiseRequested = true;
    cloudNoiseJobs.worley = std::async(std::launch::async, []() {
        return NoiseTextureGenerator3D::computeWorleyNoise(128, 128, 128);
    });
    cloudNoiseJobs.perlin = std::async(std::launch::async, []() {
        return NoiseTextureGenerator3D::computePerlinNoise(128, 128, 128);
    });
    cloudNoiseJobs.weather = std::async(std::launch::async, []() {
        return NoiseTextureGenerator3D::computeWeatherMap(512, 512);
    });
}

bool RenderSystem::cloudNoiseJobsReady() const {
    using std::chrono::seconds;
    return
        cloudNoiseJobs.worley.valid() && cloudNoiseJobs.worley.wait_for(seconds(0)) == std::future_status::ready &&
        cloudNoiseJobs.perlin.valid() && cloudNoiseJobs.perlin.wait_for(seconds(0)) == std::future_status::ready &&
        cloudNoiseJobs.weather.valid() && cloudNoiseJobs.weather.wait_for(seconds(0)) == std::future_status::ready;
}

void RenderSystem::publishCloudNoiseTextures() {
    // All futures are ready. Keep consumption and GL publication in this order;
    // noiseReady describes GPU publication, not just completed CPU work.
    worleyNoiseTexture = NoiseTextureGenerator3D::uploadVolume3D(cloudNoiseJobs.worley.get());
    perlinNoiseTexture = NoiseTextureGenerator3D::uploadVolume3D(cloudNoiseJobs.perlin.get());
    weatherMapTexture = NoiseTextureGenerator3D::uploadVolume2D(cloudNoiseJobs.weather.get());
    noiseReady = true;
}

void RenderSystem::discardCloudNoiseJobs() {
    // Shutdown joins CPU work but does not consume/rethrow worker exceptions or
    // upload any data. Reset each future only after its work has finished.
    for (auto* future : { &cloudNoiseJobs.worley, &cloudNoiseJobs.perlin, &cloudNoiseJobs.weather }) {
        if (future->valid()) {
            future->wait();
            *future = {};
        }
    }
    noiseRequested = false;
}

void RenderSystem::update(double deltaTime) {
    resizeViewportTargets();
    updateAnimations(deltaTime);
}

void RenderSystem::resizeViewportTargets() {
    // FBO sizing follows the Viewport panel content size (set via setViewportSize),
    // not the OS window. Resize all size-dependent targets when it changes.
    if ((int)width != viewportW_ || (int)height != viewportH_) {
        width = (float)viewportW_;
        height = (float)viewportH_;
        hdrColorTexture.resetSize(viewportW_, viewportH_);
        bloomBrightTexture.resetSize(viewportW_, viewportH_);
        hdrDepthTexture.resetSize(viewportW_, viewportH_);
        bloomBlurTextures[0].resetSize(viewportW_, viewportH_);
        bloomBlurTextures[1].resetSize(viewportW_, viewportH_);
        overlayColorTexture.resetSize(viewportW_, viewportH_);
        cloudRaymarchTexture.resetSize((viewportW_ + 1) / 2, (viewportH_ + 1) / 2);
        cloudTemporal.resize((viewportW_ + 1) / 2, (viewportH_ + 1) / 2);
        sceneColorTexture.resetSize(viewportW_, viewportH_);
        // Check only after all attachments have their new dimensions.
        hdrFbo.requireComplete("HDR framebuffer after resize");
        bloomBlurFbos[0].requireComplete("Bloom blur framebuffer 0 after resize");
        bloomBlurFbos[1].requireComplete("Bloom blur framebuffer 1 after resize");
        overlayFbo.requireComplete("Overlay framebuffer after resize");
        cloudRaymarchFbo.requireComplete("Cloud raymarch framebuffer after resize");
        sceneFbo.requireComplete("Scene framebuffer after resize");
    }
}

void RenderSystem::updateAnimations(double deltaTime) {
    for (auto& obj : rm_->getSceneView<AnimatorComponent>()) {
        auto animator = obj->getComponent<AnimatorComponent>();
        if (animator->playing) animator->update((float)deltaTime);
    }
}

void RenderSystem::setViewportSize(int w, int h) {
    viewportW_ = std::max(w, 1);
    viewportH_ = std::max(h, 1);
}

unsigned int RenderSystem::getSceneColorTextureId() const {
    return sceneColorTexture.ID;
}

RenderContext RenderSystem::buildContext(Camera& camera) {
    RenderContext ctx;
    ctx.camera = &camera;
    ctx.width = width;
    ctx.height = height;
    resolveShaders(ctx);
    snapshotScene(ctx);
    assignTargets(ctx);
    return ctx;
}

void RenderSystem::resolveShaders(RenderContext& ctx) {
    auto& rm = *rm_;
    ctx.forwardShader = rm.getShader(ShaderCatalog::forward.key);
    ctx.skyboxShader = rm.getShader(ShaderCatalog::skybox.key);
    ctx.directionalShadowShader = rm.getShader(ShaderCatalog::directionalShadow.key);
    ctx.compositeShader = rm.getShader(ShaderCatalog::composite.key);
    ctx.pointShadowShader = rm.getShader(ShaderCatalog::pointShadow.key);
    ctx.lightMarkerShader = rm.getShader(ShaderCatalog::lightMarker.key);
    ctx.gaussianBlurShader = rm.getShader(ShaderCatalog::gaussianBlur.key);
    ctx.skeletonShader = rm.getShader(ShaderCatalog::skeleton.key);
    ctx.cloudRaymarchShader = rm.getShader(ShaderCatalog::cloudRaymarch.key);
    ctx.cloudUpsampleShader = rm.getShader(ShaderCatalog::cloudUpsample.key);
    ctx.cloudTemporalShader = rm.getShader(ShaderCatalog::cloudTemporal.key);
    ctx.gizmoShader = rm.getShader(ShaderCatalog::gizmo.key);
}

void RenderSystem::snapshotScene(RenderContext& ctx) {
    auto& rm = *rm_;
    ctx.modelObjects = rm.getSceneView<ModelRenderComponent>();
    ctx.materialMeshObjects = rm.getSceneView<DynamicMaterialComponent>();
    ctx.pointLights = rm.getSceneView<PointLightComponent>();
    ctx.directionalLights = rm.getSceneView<DirectionalLightComponent>();
    ctx.spotLights = rm.getSceneView<SpotLightComponent>();
    // Clamp again at submission: programmatic component mutation can bypass scene insertion.
    if (ctx.pointLights.size() > RenderLimits::PointLights) ctx.pointLights.resize(RenderLimits::PointLights);
    if (ctx.directionalLights.size() > RenderLimits::DirectionalLights) ctx.directionalLights.resize(RenderLimits::DirectionalLights);
    if (ctx.spotLights.size() > RenderLimits::SpotLights) ctx.spotLights.resize(RenderLimits::SpotLights);
    ctx.skyboxes = rm.getSceneView<SkyboxComponent>();
    ctx.cloudVolumes = rm.getSceneView<CloudVolumeComponent>();
    ctx.skeletons = rm.getSceneView<SkeletonViewerComponent>();
    ctx.selectedObject = selection_ ? selection_->get() : nullptr;
    ctx.sceneIdentity = rm.getCurrentScene().get();
}

void RenderSystem::assignTargets(RenderContext& ctx) {
    ctx.cameraMatricesUbo = &cameraMatricesUbo;
    ctx.pointLightsSsbo = &pointLightsSsbo;
    ctx.directionalLightSsbo = &directionalLightSsbo;
    ctx.spotLightsSsbo = &spotLightsSsbo;
    ctx.cloudVolumesSsbo = &cloudVolumesSsbo;
    ctx.directionalShadowFbo = &directionalShadowFbo;
    ctx.pointShadowFbo = &pointShadowFbo;
    ctx.hdrFbo = &hdrFbo;
    ctx.bloomBlurFbos[0] = &bloomBlurFbos[0];
    ctx.bloomBlurFbos[1] = &bloomBlurFbos[1];
    ctx.overlayFbo = &overlayFbo;
    ctx.cloudRaymarchFbo = &cloudRaymarchFbo;
    ctx.sceneFbo = &sceneFbo;
    ctx.directionalShadowTexture = &directionalShadowTexture;
    ctx.pointShadowTexture = &pointShadowTexture;
    ctx.hdrColorTexture = &hdrColorTexture;
    ctx.bloomBrightTexture = &bloomBrightTexture;
    ctx.hdrDepthTexture = &hdrDepthTexture;
    ctx.bloomBlurTextures[0] = &bloomBlurTextures[0];
    ctx.bloomBlurTextures[1] = &bloomBlurTextures[1];
    ctx.overlayColorTexture = &overlayColorTexture;
    ctx.cloudRaymarchTexture = &cloudRaymarchTexture;
    ctx.cloudTemporal = &cloudTemporal;
    ctx.cloudRaymarchWidth = (static_cast<int>(width) + 1) / 2;
    ctx.cloudRaymarchHeight = (static_cast<int>(height) + 1) / 2;
    ctx.owner = this;
    ctx.worleyNoiseTexture = &worleyNoiseTexture;
    ctx.perlinNoiseTexture = &perlinNoiseTexture;
    ctx.weatherMapTexture = &weatherMapTexture;
    ctx.noiseReady = &noiseReady;
}

RenderContext RenderSystem::prepareFrame(Camera& camera) {
    auto ctx = buildContext(camera);
    ctx.cameraMatricesUbo->bind();
    // Re-establish indexed binding 0 before uploading this frame's camera data.
    ctx.cameraMatricesUbo->bufferBase(0);
    ctx.cameraMatricesUbo->bufferSubdata(0, sizeof(glm::mat4), glm::value_ptr(camera.getViewMat()));
    ctx.cameraMatricesUbo->bufferSubdata(sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(camera.getProjectionMat(ctx.width, ctx.height)));
    ctx.cameraMatricesUbo->unbind();
    ctx.frustum = camera.getFrustum(ctx.width, ctx.height);
    return ctx;
}

void RenderSystem::render(Camera& camera) {
    ++renderFrame_;
    ProfileFrame* capture = acquireProfileFrame();
    using Clock = std::chrono::steady_clock;
    const auto frameStart = capture ? Clock::now() : Clock::time_point{};
    if (capture) glQueryCounter(capture->queries[0], GL_TIMESTAMP);

    RenderContext ctx = prepareFrame(camera);
    if (capture) {
        capture->sample.prepareCpuMs = std::chrono::duration<double, std::milli>(Clock::now() - frameStart).count();
        glQueryCounter(capture->queries[1], GL_TIMESTAMP);
    }
    executePasses(ctx, capture);
    if (capture) {
        const double cpuMs = std::chrono::duration<double, std::milli>(Clock::now() - frameStart).count();
        submitProfileFrame(*capture, ctx, cpuMs);
    }
}

RenderSystem::ProfileFrame* RenderSystem::acquireProfileFrame() {
    if (!profilingEnabled_ || !initProfileQueries()) return nullptr;
    collectProfileResults();
    if (pendingProfiles_ < profileFrames_.size()) return &profileFrames_[profileWrite_];
    ++skippedProfileFrames_;
    return nullptr;
}

void RenderSystem::executePasses(RenderContext& ctx, ProfileFrame* capture) {
    using Clock = std::chrono::steady_clock;
    for (std::size_t i = 0; i < passes.size(); ++i) {
        auto& pass = passes[i];
        const auto passStart = capture ? Clock::now() : Clock::time_point{};
#ifdef _DEBUG
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, pass->name());
#endif
        pass->execute(ctx);
#ifdef _DEBUG
        glPopDebugGroup();
#endif
        if (capture) {
            capture->sample.passes[i].cpuMs = std::chrono::duration<double, std::milli>(Clock::now() - passStart).count();
            glQueryCounter(capture->queries[i + 2], GL_TIMESTAMP);
        }
    }
}

void RenderSystem::submitProfileFrame(ProfileFrame& capture, const RenderContext& ctx, double cpuMs) {
    auto& sample = capture.sample;
    sample.cpuMs = cpuMs;
    sample.frame = renderFrame_;
    sample.width = static_cast<int>(ctx.width);
    sample.height = static_cast<int>(ctx.height);
    sample.opaqueObjectCount = ctx.modelObjects.size() + std::count_if(ctx.materialMeshObjects.begin(), ctx.materialMeshObjects.end(),
        [](const GameObjectPtr& obj) { return !obj->has<ModelRenderComponent>() && !obj->has<CloudVolumeComponent>(); });
    sample.cloudVolumeCount = ctx.cloudVolumes.size();
    sample.pointLightCount = ctx.pointLights.size();
    sample.directionalLightCount = ctx.directionalLights.size();
    sample.spotLightCount = ctx.spotLights.size();
    profileWrite_ = (profileWrite_ + 1) % profileFrames_.size();
    ++pendingProfiles_;
}

void RenderSystem::setProfilingEnabled(bool enabled) {
    if (profilingEnabled_ == enabled) return;
    profilingEnabled_ = enabled;
    profile_ = {};
    profileRead_ = profileWrite_ = pendingProfiles_ = 0;
    skippedProfileFrames_ = 0;
}

bool RenderSystem::initProfileQueries() {
    if (!profileFrames_[0].queries.empty()) return true;
    glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &timestampBits_);
    if (timestampBits_ == 0) {
        setProfilingEnabled(false);
        TOGR_LOG_WARN("GPU timestamp queries are unavailable; render profiling disabled");
        return false;
    }
    for (auto& frame : profileFrames_) {
        frame.queries.resize(passes.size() + 2);
        frame.timestamps.resize(frame.queries.size());
        frame.sample.passes.clear();
        for (const auto& pass : passes) frame.sample.passes.push_back({pass->name()});
        glGenQueries(static_cast<GLsizei>(frame.queries.size()), frame.queries.data());
    }
    return true;
}

double RenderSystem::gpuDurationMs(GLuint64 begin, GLuint64 end) const {
    GLuint64 duration = end - begin;
    if (timestampBits_ < 64) duration &= (GLuint64(1) << timestampBits_) - 1;
    return static_cast<double>(duration) / 1000000.0;
}

void RenderSystem::collectProfileResults() {
    while (pendingProfiles_ > 0) {
        auto& frame = profileFrames_[profileRead_];
        // Check every timestamp before any RESULT read. Never read an unfinished query.
        for (const auto query : frame.queries) {
            GLint available = GL_FALSE;
            glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) return;
        }
        for (std::size_t i = 0; i < frame.queries.size(); ++i)
            glGetQueryObjectui64v(frame.queries[i], GL_QUERY_RESULT, &frame.timestamps[i]);
        auto& sample = frame.sample;
        sample.prepareGpuMs = gpuDurationMs(frame.timestamps[0], frame.timestamps[1]);
        for (std::size_t i = 0; i < sample.passes.size(); ++i)
            sample.passes[i].gpuMs = gpuDurationMs(frame.timestamps[i + 1], frame.timestamps[i + 2]);
        sample.gpuMs = gpuDurationMs(frame.timestamps.front(), frame.timestamps.back());
        sample.valid = true;
        profile_ = sample;
        profileRead_ = (profileRead_ + 1) % profileFrames_.size();
        --pendingProfiles_;
    }
}

void RenderSystem::shutdownProfileQueries() {
    for (auto& frame : profileFrames_) {
        if (!frame.queries.empty()) glDeleteQueries(static_cast<GLsizei>(frame.queries.size()), frame.queries.data());
        frame = {};
    }
    profilingEnabled_ = false;
    profile_ = {};
    profileRead_ = profileWrite_ = pendingProfiles_ = 0;
    renderFrame_ = skippedProfileFrames_ = 0;
    timestampBits_ = 0;
}

} // namespace togr
