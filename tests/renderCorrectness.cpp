#include "rendering/passes/renderPasses.hpp"
#include "rendering/renderUtils.hpp"
#include "assets/resourceManager.hpp"
#include "assets/shaderCatalog.hpp"
#include "scene/component.hpp"
#include "rendering/opengl/shader.hpp"
#include "rendering/geometry/meshGenerator.hpp"
#include "rendering/opengl/glBuffer.hpp"
#include "rendering/geometry/mesh.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

void runCloudNoiseChecks();
void runCloudSamplingChecks();

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void closeTo(float actual, float expected, const char* message, float tolerance = 0.002f) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        std::cerr << message << ": actual=" << actual << " expected=" << expected << '\n';
        throw std::runtime_error(message);
    }
}
int debugErrors = 0;
void APIENTRY debugCallback(GLenum, GLenum type, GLuint, GLenum severity, GLsizei, const GLchar* text, const void*) {
    if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR || severity == GL_DEBUG_SEVERITY_HIGH) {
        ++debugErrors; std::cerr << "GL: " << text << '\n';
    }
}
void linked(const ShaderPtr& shader) {
    GLint status = 0; glGetProgramiv(shader->ID, GL_LINK_STATUS, &status);
    check(status == GL_TRUE, "Regression shader failed to link");
}
ShaderPtr variant(const char* vertex, const char* fragment, const std::string& defines) {
    std::ifstream input(fragment); std::ostringstream stream; stream << input.rdbuf();
    auto source = stream.str();
    const auto start = source.find("#include");
    check(start != std::string::npos, "Shader settings include missing");
    source.replace(start, source.find('\n', start) - start, defines);
    std::filesystem::create_directories("x64/RenderChecks");
    const char* path = "x64/RenderChecks/variant.frag";
    { std::ofstream output(path); output << source; }
    auto shader = std::make_shared<Shader>(vertex, path); linked(shader); return shader;
}
Texture2D color(int size = 1) {
    return Texture2D(size, size, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGBA32F, GL_RGBA, GL_FLOAT);
}
glm::vec4 pixel(GLenum attachment = GL_COLOR_ATTACHMENT0, int x = 0, int y = 0) {
    glReadBuffer(attachment); glm::vec4 result;
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, &result); return result;
}
void checkLighting() {
    auto output = color(), bright = color(); FrameBuffer fbo; fbo.init(); fbo.bind();
    fbo.attachTexture2D(output, GL_COLOR_ATTACHMENT0); fbo.attachTexture2D(bright, GL_COLOR_ATTACHMENT1);
    fbo.drawBuffers({GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1});
    Texture2D depth(1, 1, GL_REPEAT, GL_NEAREST, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    float zero = 0; depth.subImage2D(0, 0, 1, 1, &zero);
    CubeMapArray cubes(1, 1, 2, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT);
    float cubeDepth[12] = {}; cubes.use(GL_TEXTURE7);
    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, 1, 1, 12, GL_DEPTH_COMPONENT, GL_FLOAT, cubeDepth);
    ShaderStorageBuffer points, dirs, spots;
    for (auto buffer : {&points, &dirs, &spots}) buffer->init();
    points.bind(); points.bufferBase(1); points.bufferData(96, nullptr);
    dirs.bind(); dirs.bufferBase(2); dirs.bufferData(32, nullptr);
    spots.bind(); spots.bufferBase(3); spots.bufferData(64, nullptr); spots.unbind();
    auto red = std::make_shared<GameObject>("Red");
    red->addComponent<Transform>()->translate = glm::vec3(0, 0, 2);
    auto r = red->addComponent<PointLightComponent>(); r->color = glm::vec3(1, 0, 0);
    r->brightness = 1; r->constant = 1; r->linear = r->quadratic = 0;
    red->addComponent<ShadowCasterCube>();
    auto blue = std::make_shared<GameObject>("Blue");
    blue->addComponent<Transform>()->translate = glm::vec3(0, 0, 2);
    auto b = blue->addComponent<PointLightComponent>(); b->color = glm::vec3(0, 0, 1);
    b->brightness = 8; b->constant = 1; b->linear = b->quadratic = 0;
    auto green = std::make_shared<GameObject>("Green");
    green->addComponent<Transform>()->rotate.y = 90;
    auto g = green->addComponent<DirectionalLightComponent>(); g->color = glm::vec3(0, 1, 0); g->brightness = 2;
    RenderHelpers::uploadPointLight(red, 0, points); RenderHelpers::uploadPointLight(blue, 1, points);
    RenderHelpers::uploadDirectionalLight(green, dirs);
    for (bool pcf : {false, true}) {
        auto shader = variant("tests/shaders/lighting.vert", ShaderCatalog::forward.fragmentPath, pcf ? "#define PCF_SHADOW\n" : "");
        shader->use(); shader->setBool("useDefaultMaterial", true); shader->setBool("hasNormalMap", false);
        shader->setBool("hasNormalMap", true);
        Material().bind(shader);
        GLint normalMap = -1;
        glGetUniformiv(shader->ID, glGetUniformLocation(shader->ID, "hasNormalMap"), &normalMap);
        check(normalMap == 0, "Default material inherited the previous object's normal map flag");
        shader->setVec3("cameraPos", glm::vec3(0, 0, 5));
        shader->setInt("pointLightCount", 2); shader->setInt("spotLightCount", 0); shader->setInt("directionalLightCount", 1);
        shader->setBool("directionalShadowEnabled", true);
        for (auto unit = GL_TEXTURE0; unit <= GL_TEXTURE4; ++unit) bindDummyTexture2D(unit);
        depth.use(GL_TEXTURE6); cubes.use(GL_TEXTURE7);
        glViewport(0, 0, 1, 1); RenderHelpers::drawFullscreenQuad();
        auto lit = pixel(); auto bloom = pixel(GL_COLOR_ATTACHMENT1);
        float albedo = std::pow(0.5f, 2.2f), term = 1 + albedo, ambient = 0.1f * albedo;
        closeTo(lit.r, 0.25f * term + ambient, "Point shadow must affect only its own light");
        closeTo(lit.g, 0.5f * term + ambient, "Directional shadow must affect only its own light");
        closeTo(lit.b, 8 * term + ambient, "Other light was incorrectly shadowed or gamma encoded");
        for (int i = 0; i < 3; ++i) closeTo(bloom[i], lit[i], "Bloom input must retain linear HDR values");
        red->getComponent<ShadowCasterCube>()->enabled = false;
        RenderHelpers::uploadPointLight(red, 0, points); shader->setBool("directionalShadowEnabled", false);
        RenderHelpers::drawFullscreenQuad(); lit = pixel();
        closeTo(lit.r, term + ambient, "Disabled point shadow sampled stale depth");
        closeTo(lit.g, 2 * term + ambient, "Disabled direction shadow sampled stale depth");
        shader->setInt("pointLightCount", 0); shader->setInt("directionalLightCount", 0);
        RenderHelpers::drawFullscreenQuad(); lit = pixel();
        for (int i = 0; i < 3; ++i) closeTo(lit[i], ambient, "Empty light accumulation is not zero initialized");
        red->getComponent<ShadowCasterCube>()->enabled = true; RenderHelpers::uploadPointLight(red, 0, points);
    }
    CubeMap environment;
    glGenTextures(1, &environment.ID); environment.use(GL_TEXTURE5);
    glm::vec4 sky(0.4f, 0.2f, 0.1f, 1), opposite(0.1f, 0.5f, 0.9f, 1);
    for (int face = 0; face < 6; ++face)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT,
            face == 5 ? &opposite : &sky);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    auto reflected = variant("tests/shaders/lighting.vert", ShaderCatalog::forward.fragmentPath, "#define USE_ENVIRONMENT_MAPPING\n");
    reflected->use(); Material().bind(reflected); reflected->setVec3("cameraPos", glm::vec3(0, 0, 5));
    reflected->setInt("pointLightCount", 0); reflected->setInt("directionalLightCount", 0); reflected->setInt("spotLightCount", 0);
    environment.use(GL_TEXTURE5); depth.use(GL_TEXTURE6); cubes.use(GL_TEXTURE7);
    RenderHelpers::drawFullscreenQuad(); auto reflection = pixel();
    float ambient = 0.1f * std::pow(0.5f, 2.2f);
    for (int c = 0; c < 3; ++c)
        closeTo(reflection[c], std::pow(sky[c], 2.2f) + ambient, "Environment reflection direction or color space is incorrect");
    auto skybox = std::make_shared<Shader>("tests/shaders/cubeSample.vert", ShaderCatalog::skybox.fragmentPath);
    linked(skybox); skybox->use(); skybox->setInt("skybox", 5); RenderHelpers::drawFullscreenQuad(); auto skyOutput = pixel();
    for (int c = 0; c < 3; ++c)
        closeTo(skyOutput[c], std::pow(sky[c], 2.2f), "Skybox must decode its display color before HDR composition");
    glUseProgram(0); fbo.unbind();
    std::cout << "PASS: per-light shadows, disabled shadows, linear HDR/Bloom, and skybox/environment color space\n";
}
void checkComposite() {
    auto hdr = color(), blur = color(), overlay = color(), result = color();
    glm::vec4 h(2, 0.5f, 0.1f, 1), b(0.3f, 0.2f, 0.1f, 1), o(0.2f, 0.1f, 0.05f, 0.4f);
    hdr.subImage2D(0, 0, 1, 1, &h); blur.subImage2D(0, 0, 1, 1, &b); overlay.subImage2D(0, 0, 1, 1, &o);
    FrameBuffer fbo; fbo.init(); fbo.bind(); fbo.attachTexture2D(result, GL_COLOR_ATTACHMENT0);
    for (bool tone : {false, true}) for (bool bloom : {false, true}) {
        auto shader = variant(ShaderCatalog::fullscreenVertexPath, ShaderCatalog::composite.fragmentPath,
            std::string(tone ? "#define USE_HDR\n" : "") + (bloom ? "#define USE_BLOOM\n" : ""));
        shader->use(); hdr.use(GL_TEXTURE0); blur.use(GL_TEXTURE1); overlay.use(GL_TEXTURE2);
        glViewport(0, 0, 1, 1); RenderHelpers::drawFullscreenQuad(); auto actual = pixel();
        for (int i = 0; i < 3; ++i) {
            float linear = h[i] * (1 - o.a) + o[i] + (bloom ? b[i] : 0);
            float mapped = tone ? 1 - std::exp(-linear) : linear;
            closeTo(actual[i], std::pow(mapped, 1 / 2.2f), "Composite must encode gamma after linear composition and tone mapping");
        }
    }
    glUseProgram(0); fbo.unbind();
    std::cout << "PASS: final composition and gamma (HDR/Bloom on/off)\n";
}
void checkOffscreenCasters(ResourceManager& resources) {
    Camera camera; RenderContext ctx; ctx.camera = &camera; ctx.frustum = camera.getFrustum(64, 64);
    auto cube = std::make_shared<GameObject>("Offscreen caster");
    cube->addComponent<Transform>()->translate = glm::vec3(0, 0, 10);
    cube->addComponent<StaticMeshComponent>()->setMesh(MeshGenerator::generateCube());
    cube->addComponent<DynamicMaterialComponent>(); ctx.materialMeshObjects = {cube};
    check(!RenderHelpers::isRenderableInFrustum(cube, ctx.frustum), "Caster fixture should be outside camera frustum");
    auto dir = std::make_shared<GameObject>("Sun"); dir->addComponent<Transform>();
    dir->addComponent<DirectionalLightComponent>(); dir->addComponent<ShadowCaster2D>(); ctx.directionalLights = {dir};
    auto point = std::make_shared<GameObject>("Lamp"); point->addComponent<Transform>();
    point->addComponent<PointLightComponent>(); point->addComponent<ShadowCasterCube>(); ctx.pointLights = {point};
    Texture2D depth(1024, 1024, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    CubeMapArray cubeDepth(1024, 1024, 1, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT);
    FrameBuffer dirFbo, pointFbo; dirFbo.init(); pointFbo.init();
    for (auto fbo : {&dirFbo, &pointFbo}) { fbo->bind(); glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE); }
    pointFbo.unbind(); ctx.directionalShadowFbo = &dirFbo; ctx.pointShadowFbo = &pointFbo;
    ctx.directionalShadowTexture = &depth; ctx.pointShadowTexture = &cubeDepth;
    ctx.directionalShadowShader = resources.getShader(ShaderCatalog::directionalShadow.key); ctx.pointShadowShader = resources.getShader(ShaderCatalog::pointShadow.key);
    bindDummyTexture2D(GL_TEXTURE8); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    DirectionalShadowPass().execute(ctx); PointShadowPass().execute(ctx);
    std::vector<float> values(1024 * 1024 * 6);
    depth.use(GL_TEXTURE6); glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, values.data());
    check(*std::min_element(values.begin(), values.begin() + 1024 * 1024) < 0.999f, "Camera culling discarded directional shadow caster");
    cubeDepth.use(GL_TEXTURE7); glGetTexImage(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT, GL_FLOAT, values.data());
    check(*std::min_element(values.begin(), values.end()) < 0.999f, "Camera culling discarded point shadow caster");
    glDisable(GL_DEPTH_TEST); glUseProgram(0);
    std::cout << "PASS: offscreen casters write directional and point shadow depth\n";
}
GameObjectPtr cloud(float z) {
    auto object = std::make_shared<GameObject>("Cloud"); auto t = object->addComponent<Transform>();
    t->translate = glm::vec3(0, 0, z); t->scale = glm::vec3(6, 6, 4);
    auto mesh = object->addComponent<StaticMeshComponent>(); mesh->aabb.min = glm::vec3(-0.5f); mesh->aabb.max = glm::vec3(0.5f);
    auto v = object->addComponent<CloudVolumeComponent>();
    v->mode = CloudVolumeComponent::Mode::LocalVolume;
    v->cloudType = 1; v->coverageLow = v->coverageHigh = 1; v->detailErosionStrength = 0;
    v->windVelocity = glm::vec3(0); v->densityMultiplier = 1;
    v->extinctionCoefficient = 0.2f; v->edgeFadeDistance = 0.1f;
    return object;
}
void checkClouds(ResourceManager& resources) {
    Camera camera; RenderContext ctx; ctx.camera = &camera; ctx.width = ctx.height = 64;
    ctx.cloudRaymarchWidth = ctx.cloudRaymarchHeight = 32;
    bool ready = true; ctx.noiseReady = &ready;
    auto cloudTarget = color(32), overlay = color(64);
    Texture2D depth(64, 64, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    std::vector<float> depthValues(64 * 64, 1); depth.subImage2D(0, 0, 64, 64, depthValues.data());
    Texture3D perlin(1, 1, 1, GL_REPEAT, GL_NEAREST, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    Texture3D worley(1, 1, 1, GL_REPEAT, GL_NEAREST, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    Texture2D weather(1, 1, GL_REPEAT, GL_NEAREST, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
    unsigned char white = 255;
    perlin.subImage3D(0, 0, 0, 1, 1, 1, &white); worley.subImage3D(0, 0, 0, 1, 1, 1, &white);
    weather.subImage2D(0, 0, 1, 1, &white);
    FrameBuffer cloudFbo, overlayFbo; cloudFbo.init(); overlayFbo.init(); cloudFbo.bind();
    cloudFbo.attachTexture2D(cloudTarget, GL_COLOR_ATTACHMENT0); overlayFbo.bind();
    overlayFbo.attachTexture2D(overlay, GL_COLOR_ATTACHMENT0); overlayFbo.unbind();
    ShaderStorageBuffer cloudVolumes; cloudVolumes.init(); ctx.cloudVolumesSsbo = &cloudVolumes;
    ctx.cloudRaymarchFbo = &cloudFbo; ctx.overlayFbo = &overlayFbo;
    ctx.cloudRaymarchTexture = &cloudTarget; ctx.hdrDepthTexture = &depth;
    ctx.worleyNoiseTexture = &worley; ctx.perlinNoiseTexture = &perlin; ctx.weatherMapTexture = &weather;
    ctx.cloudRaymarchShader = resources.getShader(ShaderCatalog::cloudRaymarch.key); ctx.cloudUpsampleShader = resources.getShader(ShaderCatalog::cloudUpsample.key);
    ctx.skeletonShader = resources.getShader(ShaderCatalog::skeleton.key);
    auto a = cloud(1), b = cloud(-5);
    auto draw = [&](std::vector<GameObjectPtr> objects) {
        ctx.cloudVolumes = std::move(objects); CloudSkeletonOverlayPass().execute(ctx);
        std::vector<glm::vec4> image(ctx.cloudRaymarchWidth * ctx.cloudRaymarchHeight); cloudTarget.use(GL_TEXTURE0);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, image.data()); return image;
    };
    auto singleA = draw({a}), singleB = draw({b}), both = draw({a, b}), reversed = draw({b, a});
    const int center = 16 * 32 + 16;
    check(singleA[center].a > 0.1f && singleB[center].a > 0.1f, "Cloud fixtures should both cover center pixel");
    closeTo(both[center].a, 1 - (1 - singleA[center].a) * (1 - singleB[center].a), "Disjoint clouds must accumulate opacity");
    for (int i = 0; i < 32 * 32; ++i) for (int c = 0; c < 4; ++c)
        closeTo(both[i][c], reversed[i][c], "Cloud image changed with insertion order");
    auto overlap = cloud(1); overlap->getComponent<CloudVolumeComponent>()->ambientStrength = 1.4f;
    auto doubled = draw({a, overlap}), reversedOverlap = draw({overlap, a});
    closeTo(doubled[center].a, 1 - std::pow(1 - singleA[center].a, 2), "Overlapping clouds must sum extinction");
    for (int i = 0; i < 32 * 32; ++i) for (int c = 0; c < 4; ++c)
        closeTo(doubled[i][c], reversedOverlap[i][c], "Overlapping cloud radiance changed with insertion order");
    auto partial = cloud(-1); auto partialOnly = draw({partial});
    auto partialBoth = draw({a, partial}), partialReversed = draw({partial, a});
    closeTo(partialBoth[center].a, 1 - (1 - singleA[center].a) * (1 - partialOnly[center].a),
        "Partially overlapping clouds lost ray segments", 0.008f);
    for (int i = 0; i < 32 * 32; ++i) for (int c = 0; c < 4; ++c)
        closeTo(partialBoth[i][c], partialReversed[i][c], "Partial overlap changed with insertion order");
    a->getComponent<Transform>()->scale.x *= -1; auto mirrored = draw({a});
    for (int c = 0; c < 4; ++c) closeTo(mirrored[center][c], singleA[center][c], "Negative cloud scale reversed bounds");
    a->getComponent<Transform>()->scale.x = 0; auto empty = draw({a});
    closeTo(empty[center].a, 0, "Zero-size cloud must be skipped");
    a->getComponent<Transform>()->scale.x = 6;
    auto clip = camera.getProjectionMat(64, 64) * camera.getViewMat() * glm::vec4(0, 0, 4, 1);
    std::fill(depthValues.begin(), depthValues.end(), clip.z / clip.w * 0.5f + 0.5f);
    depth.subImage2D(0, 0, 64, 64, depthValues.data()); auto occluded = draw({a, b});
    closeTo(occluded[center].a, 0, "Opaque geometry must terminate cloud ray");
    glUseProgram(0);
    std::cout << "PASS: multi-cloud order, overlap, negative/zero scale, and opaque depth termination\n";

    // Odd dimensions put the middle ray exactly on the camera's forward axis,
    // exercising the horizontal slab case without an artificial vertical epsilon.
    ctx.cloudRaymarchWidth = ctx.cloudRaymarchHeight = 33;
    cloudTarget.resetSize(33, 33);
    const int skyCenter = 16 * 33 + 16;
    std::fill(depthValues.begin(), depthValues.end(), 1.0f);
    depth.subImage2D(0, 0, 64, 64, depthValues.data());
    auto sky = std::make_shared<GameObject>("Sky layer");
    auto layer = sky->addComponent<CloudVolumeComponent>();
    check(layer->mode == CloudVolumeComponent::Mode::SkyLayer, "New clouds should default to sky layers");
    layer->layerBottom = 23; layer->layerThickness = 67;
    layer->maxTraceDistance = 4321; layer->distanceFadeStart = 3456;
    layer->windVelocity = glm::vec3(3, 0, -2); layer->viewStepWorldSize = 7; layer->edgeFadeDistance = 4;
    layer->viewMaxSteps = 128; layer->lightSteps = 12; layer->emptyStepMultiplier = 3; layer->temporalAccumulation = false;
    for (auto preset : {CloudVolumeComponent::Preset::ScatteredCumulus,
            CloudVolumeComponent::Preset::BrokenStratocumulus, CloudVolumeComponent::Preset::Overcast}) {
        layer->applyPreset(preset);
        check(layer->layerBottom == 23 && layer->layerThickness == 67 && layer->maxTraceDistance == 4321 &&
            layer->distanceFadeStart == 3456 && layer->windVelocity == glm::vec3(3, 0, -2) &&
            layer->viewStepWorldSize == 7 && layer->edgeFadeDistance == 4 && layer->mode == CloudVolumeComponent::Mode::SkyLayer &&
            layer->viewMaxSteps == 128 && layer->lightSteps == 12 && layer->emptyStepMultiplier == 3 && !layer->temporalAccumulation,
            "Appearance preset changed the layer bounds, wind, mode or quality");
    }
    layer->applyPreset(CloudVolumeComponent::Preset::ScatteredCumulus);
    layer->viewStepWorldSize = 2;
    layer->viewMaxSteps = 96; layer->lightSteps = 8; layer->emptyStepMultiplier = 4; layer->temporalAccumulation = true;
    layer->layerBottom = 5; layer->layerThickness = 6;
    layer->maxTraceDistance = 1000; layer->distanceFadeStart = 800;
    layer->cloudType = 1; layer->coverageLow = layer->coverageHigh = 1;
    // Analytic fixtures deliberately use a horizontally homogeneous medium.
    layer->regionCoverage = 1; layer->densityVariation = layer->heightVariation = 0;
    layer->detailErosionStrength = 0; layer->windVelocity = glm::vec3(0);
    layer->extinctionCoefficient = 0.2f; layer->densityMultiplier = 1;
    auto placeCamera = [&](glm::vec3 position, float pitch = 0.0f) {
        camera = Camera(position); camera.lookDelta(0, -pitch / DEFAULT_SENSITIVITY);
    };
    auto finiteImage = [&](const std::vector<glm::vec4>& image) {
        for (const auto& p : image) {
            for (int c = 0; c < 4; ++c) check(std::isfinite(p[c]), "Sky layer produced NaN or infinity");
            check(p.a >= 0 && p.a <= 1, "Sky layer opacity outside [0,1]");
        }
    };
    placeCamera(glm::vec3(0, 0, 5), 60);
    auto overhead = draw({sky}); finiteImage(overhead);
    check(overhead[skyCenter].a > 0.1f, "Sky layer without mesh or Transform should cover the sky");
    for (auto offset : {glm::vec3(12000, 0, 0), glm::vec3(-12000, 0, 0),
            glm::vec3(0, 0, 12000), glm::vec3(0, 0, -12000)}) {
        placeCamera(glm::vec3(0, 0, 5) + offset, 60);
        auto distant = draw({sky}); finiteImage(distant);
        check(distant[skyCenter].a > 0.1f, "Sky coverage disappeared beyond the old XZ cloud bounds");
        closeTo(distant[skyCenter].a, overhead[skyCenter].a, "Uniform sky coverage changed after horizontal travel", 0.015f);
    }
    auto ignoredTransform = sky->addComponent<Transform>();
    ignoredTransform->translate = glm::vec3(50000); ignoredTransform->scale = glm::vec3(0);
    placeCamera(glm::vec3(0, 0, 5), 60);
    auto independent = draw({sky});
    closeTo(independent[skyCenter].a, overhead[skyCenter].a, "Sky altitude depended on the object's Transform");
    auto local = cloud(0); local->getComponent<Transform>()->translate.y = 8;
    auto mixed = draw({sky, local}), mixedReversed = draw({local, sky});
    check(mixed[skyCenter].a > overhead[skyCenter].a, "Local volume did not accumulate with the sky layer");
    for (int i = 0; i < 33 * 33; ++i) for (int c = 0; c < 4; ++c)
        closeTo(mixed[i][c], mixedReversed[i][c], "Mixed sky/local media changed with insertion order");
    placeCamera(glm::vec3(0, 20, 5), -60);
    auto fromAbove = draw({sky}); finiteImage(fromAbove);
    check(fromAbove[skyCenter].a > 0.1f, "Sky layer could not be viewed from above");
    placeCamera(glm::vec3(0, 0, 5));
    auto belowHorizontal = draw({sky}); finiteImage(belowHorizontal);
    closeTo(belowHorizontal[skyCenter].a, 0, "Horizontal ray below the layer should miss it");
    placeCamera(glm::vec3(0, 0, 5), -60);
    closeTo(draw({sky})[skyCenter].a, 0, "Ray looking away from sky layer should miss it");
    placeCamera(glm::vec3(0, 8, 5));
    auto insideHorizontal = draw({sky}); finiteImage(insideHorizontal);
    check(insideHorizontal[skyCenter].a > 0.9f, "Horizontal ray inside the layer should integrate to its distance limit");
    layer->maxTraceDistance = 12; layer->distanceFadeStart = 12;
    auto bounded = draw({sky}); finiteImage(bounded);
    closeTo(bounded[skyCenter].a, 1 - std::exp(-0.2f * 12), "Sky ray did not respect maximum tracing distance");
    // Exact homogeneous solution: sigma_s * constant sky radiance integrates to
    // albedo * incidentRadiance * (1-T), independently of coefficient or steps.
    auto blackSun = std::make_shared<GameObject>("Medium regression sun");
    blackSun->addComponent<Transform>(); blackSun->addComponent<DirectionalLightComponent>()->color = glm::vec3(0);
    ctx.directionalLights = {blackSun};
    layer->ambientStrength = 1; layer->multipleScattering = 0; layer->scatteringAlbedo = 0.8f;
    const glm::vec3 ambientAtMidHeight(0.8f, 0.85f, 0.91f);
    auto homogeneous = [&] {
        auto result = draw({sky})[skyCenter];
        float alpha = 1 - std::exp(-layer->extinctionCoefficient * layer->densityMultiplier * 12);
        closeTo(result.a, alpha, "Homogeneous medium extinction is incorrect");
        for (int c = 0; c < 3; ++c)
            closeTo(result[c], alpha * layer->scatteringAlbedo * ambientAtMidHeight[c],
                "Cloud source term does not include sigma_s or lost premultiplied alpha");
        return result;
    };
    homogeneous();
    layer->densityMultiplier = 2; const auto dense = homogeneous();
    layer->densityMultiplier = 1; layer->extinctionCoefficient = 0.4f; const auto equivalent = homogeneous();
    for (int c = 0; c < 4; ++c) closeTo(dense[c], equivalent[c], "Density and extinction coefficient are inconsistent");
    for (float spacing : {0.25f, 16.0f}) { layer->viewStepWorldSize = spacing; homogeneous(); }
    layer->viewStepWorldSize = 2; layer->extinctionCoefficient = 0.2f;
    layer->densityMultiplier = 0.001f; homogeneous(); // retain optically thin media
    layer->densityMultiplier = 1; layer->scatteringAlbedo = 0; homogeneous(); // absorption only
    layer->scatteringAlbedo = 0.8f;
    layer->extinctionCoefficient = 0; homogeneous(); // zero opacity and emission
    layer->extinctionCoefficient = 0.2f; layer->densityMultiplier = 0; homogeneous();
    layer->densityMultiplier = 1; layer->coverageLow = layer->coverageHigh = 0;
    auto clearSky = draw({sky})[skyCenter];
    for (int c = 0; c < 4; ++c) closeTo(clearSky[c], 0, "Zero coverage must leave clear sky");
    layer->coverageLow = layer->coverageHigh = 1;
    // Isotropic scattering toward a horizontal sun has a constant incident
    // radiance exp(-sigma_t * lightDistance) / 4 in this engine's light units.
    blackSun->getComponent<DirectionalLightComponent>()->color = glm::vec3(1);
    blackSun->getComponent<Transform>()->rotate.y = -90;
    layer->ambientStrength = 0; layer->sunStrength = 1; layer->phaseForwardG = 0; layer->scatteringAlbedo = 1;
    auto isotropic = draw({sky})[skyCenter];
    for (int c = 0; c < 3; ++c)
        closeTo(isotropic[c], 0.25f * std::exp(-0.2f * 12) * (1 - std::exp(-0.2f * 12)),
            "Direct scattering must use the normalized phase and the same shadow extinction");
    layer->sunStrength = 2; auto strongerSun = draw({sky})[skyCenter];
    for (int c = 0; c < 3; ++c) closeTo(strongerSun[c], isotropic[c] * 2, "SunStrength did not scale only the incident sun radiance");
    closeTo(strongerSun.a, isotropic.a, "SunStrength changed cloud opacity");
    layer->sunStrength = 1; layer->phaseForwardG = 0.65f; auto forward = draw({sky})[skyCenter];
    check(forward.r > isotropic.r * 2, "Forward phase did not brighten the sunward cloud");
    layer->multipleScattering = 1; auto higherOrders = draw({sky})[skyCenter];
    check(higherOrders.r > forward.r, "Higher scattering orders did not brighten the shadowed cloud");
    closeTo(higherOrders.a, isotropic.a, "Phase or multiple scattering changed extinction");
    layer->sunStrength = 3;
    layer->ambientStrength = 0.7f; layer->multipleScattering = 0.45f; layer->scatteringAlbedo = 0.95f;
    ctx.directionalLights.clear();
    std::cout << "PASS: cloud presets preserve placement; analytic density/extinction/scattering, absorption, step invariance, sun, phase and higher orders\n";
    auto horizonSun = std::make_shared<GameObject>("Horizon sun");
    auto sunTransform = horizonSun->addComponent<Transform>();
    horizonSun->addComponent<DirectionalLightComponent>(); ctx.directionalLights = {horizonSun};
    for (float elevation : {0.0f, -0.001f, 0.001f}) {
        sunTransform->rotate.z = elevation;
        auto horizonLit = draw({sky}); finiteImage(horizonLit);
        closeTo(horizonLit[skyCenter].a, bounded[skyCenter].a, "Low-angle sun changed view extinction");
    }
    ctx.directionalLights.clear();
    layer->distanceFadeStart = 0;
    auto faded = draw({sky}); finiteImage(faded);
    closeTo(faded[skyCenter].a, 1 - std::exp(-0.2f * 6), "Sky distance fade did not smoothly reduce optical depth", 0.02f);
    check(faded[skyCenter].a < bounded[skyCenter].a - 0.1f, "Distance fade had no visible effect");
    layer->maxTraceDistance = 1000; layer->distanceFadeStart = 800;
    auto opaqueClip = camera.getProjectionMat(64, 64) * camera.getViewMat() * glm::vec4(0, 8, 4, 1);
    std::fill(depthValues.begin(), depthValues.end(), opaqueClip.z / opaqueClip.w * 0.5f + 0.5f);
    depth.subImage2D(0, 0, 64, 64, depthValues.data());
    auto skyOccluded = draw({sky}); finiteImage(skyOccluded);
    closeTo(skyOccluded[skyCenter].a, 1 - std::exp(-0.2f), "Opaque surface did not terminate the sky-layer ray");
    std::fill(depthValues.begin(), depthValues.end(), 1.0f);
    depth.subImage2D(0, 0, 64, 64, depthValues.data());
    layer->layerThickness = 0;
    closeTo(draw({sky})[skyCenter].a, 0, "Zero-thickness sky layer must be skipped");
    layer->layerThickness = -6;
    closeTo(draw({sky})[skyCenter].a, 0, "Negative-thickness sky layer must be skipped");
    layer->layerThickness = 6; layer->maxTraceDistance = 0;
    closeTo(draw({sky})[skyCenter].a, 0, "Zero tracing budget must skip a sky layer");
    layer->maxTraceDistance = 1; layer->distanceFadeStart = 1;
    placeCamera(glm::vec3(0, 0, 5), 60);
    closeTo(draw({sky})[skyCenter].a, 0, "Layer entry beyond the tracing budget should be rejected");

    // Two cameras on the same ray see the same world-space medium. Procedural
    // weather detects accidental camera-relative coordinates in the sky field.
    layer->maxTraceDistance = 1000; layer->distanceFadeStart = 800;
    layer->coverageLow = 0.2f;
    layer->weatherScale = 0.02f;
    auto anchored = draw({sky});
    check(anchored[skyCenter].a > 0.01f, "World anchoring fixture must contain visible sky density");
    const glm::vec3 behind = camera.getPos() - camera.getFront() * 400.0f;
    placeCamera(behind, 60);
    auto sameWorldRay = draw({sky}); finiteImage(sameWorldRay);
    for (int c = 0; c < 4; ++c)
        closeTo(sameWorldRay[skyCenter][c], anchored[skyCenter][c], "Sky weather moved with the camera", 0.01f);
    glUseProgram(0);
    std::cout << "PASS: sky-layer travel, mixed media, above/inside/parallel rays, horizon sun, tracing limit, fade, depth, and world anchoring\n";
}

void checkMeshCaches() {
    for (int type = 0; type < 3; ++type) {
        auto mesh = type == 0 ? MeshGenerator::generateCube() : type == 1 ? MeshGenerator::generatePlane() : MeshGenerator::generateSphere();
        std::weak_ptr<Mesh> observer = mesh;
        mesh.reset();
        check(observer.expired(), "Generated mesh cache retained GL resources after last owner released");
    }
    auto coarse = MeshGenerator::generateSphere(8, 4);
    auto fine = MeshGenerator::generateSphere(16, 8);
    check(coarse != fine, "Sphere cache ignored tessellation parameters");
    check(MeshGenerator::generateSphere(8, 4) == coarse, "Matching sphere parameters did not reuse the live mesh");
    check(coarse->vertices.size() == 9 * 5 && coarse->indices.size() == 6 * 8 * 3,
        "Coarse sphere topology does not match requested resolution");
    check(fine->vertices.size() == 17 * 9 && fine->indices.size() == 6 * 16 * 7,
        "Fine sphere topology does not match requested resolution");
    for (const auto& mesh : {coarse, fine}) {
        for (auto index : mesh->indices) check(index < mesh->vertices.size(), "Sphere index out of bounds");
    }
    for (const auto parameters : {std::pair<int, int>{0, 4}, {2, 4}, {8, 0}, {8, 1}, {-1, 4}}) {
        bool rejected = false;
        try { MeshGenerator::generateSphere(parameters.first, parameters.second); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Invalid sphere tessellation was not rejected");
    }
    bool tooLarge = false;
    try { MeshGenerator::generateSphere(100000, 100000); }
    catch (const std::length_error&) { tooLarge = true; }
    check(tooLarge, "Oversized sphere tessellation was not rejected");
    std::weak_ptr<Mesh> coarseObserver = coarse, fineObserver = fine;
    coarse.reset(); fine.reset();
    check(coarseObserver.expired() && fineObserver.expired(), "Sphere cache retained mesh owners");
    check(MeshGenerator::generateSphere(8, 4)->vertices.size() == 9 * 5, "Expired sphere cache could not regenerate mesh");
    std::cout << "PASS: mesh cache lifetime, sphere resolutions, sharing, and invalid parameters\n";
}

// Small uncompressed TGA fixtures exercise stb's real 1/2/3/4-channel path.
void writeImage(const std::filesystem::path& path, int width, int height, int channels,
    const std::vector<unsigned char>& pixels) {
    unsigned char header[18] = {};
    header[2] = channels <= 2 ? 3 : 2;
    header[12] = static_cast<unsigned char>(width); header[14] = static_cast<unsigned char>(height);
    header[16] = static_cast<unsigned char>(channels * 8);
    header[17] = static_cast<unsigned char>(0x20 | ((channels == 2 || channels == 4) ? 8 : 0));
    auto stored = pixels;
    if (channels >= 3) {
        for (std::size_t i = 0; i < stored.size(); i += channels) std::swap(stored[i], stored[i + 2]);
    }
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(header), sizeof(header));
    output.write(reinterpret_cast<const char*>(stored.data()), stored.size());
    check(bool(output), "Texture fixture write failed");
}

void checkTextureUploads() {
    const std::filesystem::path directory = "x64/RenderChecks/textures";
    std::filesystem::create_directories(directory);
    GLint packAlignment, unpackAlignment;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment); glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1); glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
    auto restored = [] {
        GLint alignment; glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        check(alignment == 8, "Texture upload leaked pixel-store alignment");
    };
    const GLenum formats[] = {GL_RED, GL_RG, GL_RGB, GL_RGBA};
    for (int channels = 1; channels <= 4; ++channels) {
        std::vector<unsigned char> source(3 * 2 * channels);
        for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<unsigned char>(17 + i * 5);
        const auto path = directory / (std::to_string(channels) + ".tga");
        writeImage(path, 3, 2, channels, source);
        Texture2D texture(path.string().c_str(), Texture2D::Type::ALBEDO, GL_CLAMP_TO_EDGE, GL_NEAREST);
        check(texture.ID != 0, "Texture fixture load failed"); restored();
        GLint width = 0, height = 0;
        glBindTexture(GL_TEXTURE_2D, texture.ID);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        check(width == 3 && height == 2, "Loaded texture size mismatch");
        std::vector<unsigned char> actual(source.size()), expected(source);
        std::rotate(expected.begin(), expected.begin() + 3 * channels, expected.end());
        glGetTexImage(GL_TEXTURE_2D, 0, formats[channels - 1], GL_UNSIGNED_BYTE, actual.data());
        check(actual == expected, "Loaded texture channel data or row alignment mismatch");
        texture.resetSize(3, 2);
        texture.subImage2D(0, 0, 3, 2, source.data()); restored();
        glBindTexture(GL_TEXTURE_2D, texture.ID);
        glGetTexImage(GL_TEXTURE_2D, 0, formats[channels - 1], GL_UNSIGNED_BYTE, actual.data());
        check(actual == source, "Image texture lost upload format after resize");

        const auto cubeDirectory = directory / ("cube" + std::to_string(channels));
        std::filesystem::create_directories(cubeDirectory);
        source.resize(3 * 3 * channels);
        for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<unsigned char>(11 + i * 3);
        const char* faces[] = {"right.jpg", "left.jpg", "top.jpg", "bottom.jpg", "front.jpg", "back.jpg"};
        for (const auto face : faces) writeImage(cubeDirectory / face, 3, 3, channels, source);
        CubeMap cube(cubeDirectory.string().c_str(), GL_CLAMP_TO_EDGE, GL_NEAREST); restored();
        check(cube.ID != 0, "Cubemap fixture load failed");
        glBindTexture(GL_TEXTURE_CUBE_MAP, cube.ID); actual.resize(source.size());
        for (int face = 0; face < 6; ++face) {
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, formats[channels - 1], GL_UNSIGNED_BYTE, actual.data());
            check(actual == source, "Cubemap channel data or row alignment mismatch");
        }
    }
    for (const auto internalFormat : {GL_RGBA8, GL_RGBA32F}) {
        Texture2D texture(3, 2, GL_CLAMP_TO_BORDER, GL_NEAREST, internalFormat, GL_RGBA,
            internalFormat == GL_RGBA8 ? GL_UNSIGNED_BYTE : GL_FLOAT);
        texture.setBorderColor(0.25f, 0.5f, 0.75f, 1.0f);
        for (int i = 0; i < 100; ++i) texture.setDefaultColor(0.25f, 0.5f, 0.75f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, texture.ID);
        float border[4]; glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        std::vector<float> pixels(3 * 2 * 4); glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
        for (int channel = 0; channel < 4; ++channel) {
            closeTo(border[channel], (channel + 1) * 0.25f, "Texture border color mismatch");
            for (int pixel = 0; pixel < 6; ++pixel)
                closeTo(pixels[pixel * 4 + channel], (channel + 1) * 0.25f, "Texture clear source type mismatch");
        }
    }
    {
        Texture3D texture(3, 2, 2, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_R8, GL_RED, GL_UNSIGNED_BYTE);
        std::vector<unsigned char> data(12), actual(12);
        for (int i = 0; i < 12; ++i) data[i] = static_cast<unsigned char>(i * 7);
        texture.subImage3D(0, 0, 0, 3, 2, 2, data.data()); restored();
        glBindTexture(GL_TEXTURE_3D, texture.ID); glGetTexImage(GL_TEXTURE_3D, 0, GL_RED, GL_UNSIGNED_BYTE, actual.data());
        check(actual == data, "3D texture row or slice alignment mismatch");
    }
    {
        Texture3D texture(3, 2, 2, GL_CLAMP_TO_EDGE, GL_NEAREST, GL_RGBA32F, GL_RGBA, GL_FLOAT);
        std::vector<float> data(48), actual(48);
        for (int i = 0; i < 48; ++i) data[i] = i * 0.125f;
        texture.subImage3D(0, 0, 0, 3, 2, 2, data.data()); restored();
        glBindTexture(GL_TEXTURE_3D, texture.ID); glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, actual.data());
        check(actual == data, "3D texture ignored constructor format or source type");
    }
    glBindTexture(GL_TEXTURE_2D, 0); glBindTexture(GL_TEXTURE_3D, 0); glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, packAlignment); glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
    std::cout << "PASS: texture channels, odd-width rows, cubemap faces, typed clears, and 3D uploads\n";
}
}

void runRenderCorrectnessChecks() {
    check(glfwInit() != 0, "GLFW initialization failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5); glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    auto window = glfwCreateWindow(64, 64, "Render correctness checks", nullptr, nullptr);
    check(window != nullptr, "Hidden GL context failed"); glfwMakeContextCurrent(window);
    check(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0, "GL loader failed");
    glEnable(GL_DEBUG_OUTPUT); glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); glDebugMessageCallback(debugCallback, nullptr);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_FRAMEBUFFER_SRGB);
    {
        ResourceManager resources; resources.init();
        checkLighting(); checkComposite(); checkOffscreenCasters(resources); checkClouds(resources);
        runCloudNoiseChecks(); runCloudSamplingChecks(); checkMeshCaches(); checkTextureUploads();
        resources.shutdown(); RenderHelpers::shutdownSharedGeometry(); destroyDummyTexture2D();
    }
    check(glGetError() == GL_NO_ERROR && debugErrors == 0, "GL errors during rendering regression checks");
    glfwDestroyWindow(window); glfwTerminate();
}
