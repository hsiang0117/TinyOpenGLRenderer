#include "app/engine.hpp"
#include "assets/shaderCatalog.hpp"
#include "scene/gameObjectFactory.hpp"
#include "rendering/renderUtils.hpp"
#include "animation/assimpNode.hpp"
#include "animation/bone.hpp"
#include "scene/component.hpp"
#include "scene/scene.hpp"
#include "rendering/renderLimits.hpp"
#include "rendering/opengl/shader.hpp"
#include <assimp/anim.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void requireNoGlError() {
    require(glGetError() == GL_NO_ERROR, "OpenGL error during safety checks");
}

// Observe real deletion calls, including destructor calls after context teardown.
int lateDeletes = 0;
int deletedTextures = 0, deletedBuffers = 0, deletedFramebuffers = 0, deletedVertexArrays = 0;
int deletedPrograms = 0;
PFNGLDELETETEXTURESPROC originalDeleteTextures;
PFNGLDELETEBUFFERSPROC originalDeleteBuffers;
PFNGLDELETEFRAMEBUFFERSPROC originalDeleteFramebuffers;
PFNGLDELETEVERTEXARRAYSPROC originalDeleteVertexArrays;
PFNGLDELETEPROGRAMPROC originalDeleteProgram;

void APIENTRY trackDeleteTextures(GLsizei count, const GLuint* names) {
    if (!glfwGetCurrentContext()) { ++lateDeletes; return; }
    deletedTextures += count;
    originalDeleteTextures(count, names);
}
void APIENTRY trackDeleteBuffers(GLsizei count, const GLuint* names) {
    if (!glfwGetCurrentContext()) { ++lateDeletes; return; }
    deletedBuffers += count;
    originalDeleteBuffers(count, names);
}
void APIENTRY trackDeleteFramebuffers(GLsizei count, const GLuint* names) {
    if (!glfwGetCurrentContext()) { ++lateDeletes; return; }
    deletedFramebuffers += count;
    originalDeleteFramebuffers(count, names);
}
void APIENTRY trackDeleteVertexArrays(GLsizei count, const GLuint* names) {
    if (!glfwGetCurrentContext()) { ++lateDeletes; return; }
    deletedVertexArrays += count;
    originalDeleteVertexArrays(count, names);
}
void APIENTRY trackDeleteProgram(GLuint name) {
    if (!glfwGetCurrentContext()) { ++lateDeletes; return; }
    ++deletedPrograms;
    originalDeleteProgram(name);
}

void observeDeletes() {
    deletedTextures = deletedBuffers = deletedFramebuffers = deletedVertexArrays = 0;
    deletedPrograms = 0;
    originalDeleteTextures = glad_glDeleteTextures;
    originalDeleteBuffers = glad_glDeleteBuffers;
    originalDeleteFramebuffers = glad_glDeleteFramebuffers;
    originalDeleteVertexArrays = glad_glDeleteVertexArrays;
    originalDeleteProgram = glad_glDeleteProgram;
    glad_glDeleteTextures = trackDeleteTextures;
    glad_glDeleteBuffers = trackDeleteBuffers;
    glad_glDeleteFramebuffers = trackDeleteFramebuffers;
    glad_glDeleteVertexArrays = trackDeleteVertexArrays;
    glad_glDeleteProgram = trackDeleteProgram;
}

void checkKeyframes() {
    aiNodeAnim emptyChannel;
    Bone empty(&emptyChannel);
    empty.update(10.0f);
    require(empty.getLocalTransform() == glm::mat4(1.0f), "Empty keyframes must be safe");

    aiNodeAnim channel;
    channel.mNumPositionKeys = channel.mNumRotationKeys = channel.mNumScalingKeys = 2;
    channel.mPositionKeys = new aiVectorKey[2];
    channel.mRotationKeys = new aiQuatKey[2];
    channel.mScalingKeys = new aiVectorKey[2];
    for (int i = 0; i < 2; ++i) {
        channel.mPositionKeys[i].mTime = channel.mRotationKeys[i].mTime = channel.mScalingKeys[i].mTime = i;
        channel.mPositionKeys[i].mValue = aiVector3D(i * 4.0f, 0, 0);
        channel.mRotationKeys[i].mValue = aiQuaternion();
        channel.mScalingKeys[i].mValue = aiVector3D(i + 1.0f);
    }
    Bone bone(&channel);
    for (float time : { 1.0f, 2.0f, 100.0f }) {
        bone.update(time);
        require(std::abs(bone.getLocalTransform()[3].x - 4.0f) < 0.0001f, "Last keyframe must clamp safely");
    }
    bone.update(-1.0f);
    require(std::abs(bone.getLocalTransform()[3].x) < 0.0001f, "Before-first keyframe must clamp safely");
    channel.mPositionKeys[1].mTime = channel.mRotationKeys[1].mTime = channel.mScalingKeys[1].mTime = 0;
    Bone duplicateTimes(&channel);
    duplicateTimes.update(1.0f);
    require(std::isfinite(duplicateTimes.getLocalTransform()[0].x), "Duplicate timestamps must stay finite");
}

template<typename T>
void checkLightCapacity(std::size_t capacity) {
    Scene scene;
    for (std::size_t i = 0; i < capacity; ++i) {
        auto object = std::make_shared<GameObject>("Light");
        object->addComponent<T>();
        require(scene.addObject(object), "Light within capacity was rejected");
    }
    auto excess = std::make_shared<GameObject>("Excess light");
    excess->addComponent<T>();
    require(!scene.addObject(excess), "Light exceeding capacity was accepted");
    require(scene.getView<T>().size() == capacity, "Rejected light changed the scene");
    scene.removeObjectByIndex(0);
    require(scene.addObject(excess), "Removing a light did not free capacity");
}

Node makeNode(int id, int parent) {
    Node node{};
    node.id = id;
    node.parentIndex = parent;
    node.name = std::to_string(id);
    node.transform = node.offsetMatrix = glm::mat4(1.0f);
    node.position = glm::vec3(0.0f);
    node.isBoneNode = true;
    return node;
}

void checkAnimationAndSkeleton(ResourceManager& resources) {
    aiAnimation rawAnimation;
    std::vector<Node> emptyNodes;
    Animation animation(&rawAnimation, emptyNodes);
    Animator emptyAnimator(&animation, emptyNodes);
    emptyAnimator.updateAnimation(1.0f);
    Animation invalid(nullptr, emptyNodes);
    emptyAnimator.playAnimation(&invalid);
    emptyAnimator.updateAnimation(1.0f);

    std::vector<Node> nodes = { makeNode(0, -1), makeNode(1, 0), makeNode(2, 999), makeNode(3, -2) };
    nodes[0].transform = glm::translate(glm::mat4(1.0f), glm::vec3(2, 0, 0));
    nodes[1].transform = glm::translate(glm::mat4(1.0f), glm::vec3(3, 0, 0));
    nodes[0].childrenIndices = { 1, -1, 999 };
    Animator animator(&animation, nodes);
    animator.updateAnimation(1.0f); // Zero duration, no animated channels.
    require(std::abs(nodes[1].position.x - 5.0f) < 0.0001f, "Static ancestor transform was lost");

    auto shader = resources.getShader(ShaderCatalog::skeleton.key);
    shader->use();
    shader->setMat4("model", glm::mat4(1.0f));
    SkeletonViewerComponent skeleton(nodes);
    skeleton.show = true;
    skeleton.drawSkeleton(); // Root and invalid parent indices must not index the vector.
    AnimatorComponent noAnimations(nodes);
    require(noAnimations.getAnimationsNames().empty(), "Unassigned animation pointer is unsafe");
    requireNoGlError();
}

template<typename T>
void addLightsAfterInsertion(Scene& scene, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        auto object = std::make_shared<GameObject>("Programmatic light");
        object->addComponent<Transform>();
        if constexpr (std::is_same_v<T, PointLightComponent>) object->addComponent<ShadowCasterCube>();
        if constexpr (std::is_same_v<T, DirectionalLightComponent>) object->addComponent<ShadowCaster2D>();
        require(scene.addObject(object), "Non-light insertion failed");
        object->addComponent<T>(); // Deliberately bypass insertion limits to test submission clamping.
    }
}

void checkRenderer() {
    require(glfwInit() != 0, "GLFW initialization failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    auto* window = glfwCreateWindow(64, 64, "Safety checks", nullptr, nullptr);
    require(window != nullptr, "Hidden OpenGL 4.5 context unavailable");
    glfwMakeContextCurrent(window);
    {
        RenderSystem renderer;
        renderer.init();
        observeDeletes();
        ResourceManager resources;
        resources.init();
        for (auto name : { ShaderCatalog::forward.key, ShaderCatalog::skybox.key, ShaderCatalog::directionalShadow.key,
            ShaderCatalog::composite.key, ShaderCatalog::pointShadow.key, ShaderCatalog::lightMarker.key,
            ShaderCatalog::gaussianBlur.key, ShaderCatalog::skeleton.key, ShaderCatalog::cloudRaymarch.key,
            ShaderCatalog::cloudUpsample.key, ShaderCatalog::cloudTemporal.key, ShaderCatalog::gizmo.key }) {
            auto shader = resources.getShader(name);
            require(shader != nullptr, "Shader registration missing");
            GLint linked = 0;
            glGetProgramiv(shader->ID, GL_LINK_STATUS, &linked);
            require(linked == GL_TRUE, "Renderer shader failed to link");
        }
        SelectionModel selection;
        renderer.setResourceManager(&resources);
        renderer.setSelectionModel(&selection);
        renderer.setViewportSize(64, 64);
        renderer.update(0);
        checkAnimationAndSkeleton(resources);

        auto scene = resources.getCurrentScene();
        addLightsAfterInsertion<PointLightComponent>(*scene, RenderLimits::PointLights + 2);
        addLightsAfterInsertion<SpotLightComponent>(*scene, RenderLimits::SpotLights + 2);
        addLightsAfterInsertion<DirectionalLightComponent>(*scene, RenderLimits::DirectionalLights + 1);
        selection.set(scene->getObjectAt(0));
        Camera camera;
        renderer.render(camera);
        requireNoGlError();
        auto shader = resources.getShader(ShaderCatalog::forward.key);
        for (auto count : { std::make_pair("pointLightCount", RenderLimits::PointLights),
            std::make_pair("spotLightCount", RenderLimits::SpotLights),
            std::make_pair("directionalLightCount", RenderLimits::DirectionalLights) }) {
            GLint uploaded = -1;
            glGetUniformiv(shader->ID, glGetUniformLocation(shader->ID, count.first), &uploaded);
            require(uploaded == static_cast<GLint>(count.second), "Shader light count exceeded GPU capacity");
        }
        shader.reset();
        scene.reset();
        selection.clear();
        const auto colorTexture = renderer.getSceneColorTextureId();
        require(glIsTexture(colorTexture), "Scene texture was never created");
        std::cout << "PASS: animation, skeleton, and GPU light limits\n" << std::flush;

        renderer.ensureCloudNoise(true); // Shutdown must join outstanding CPU workers.
        renderer.shutdown();
        require(!glIsTexture(colorTexture), "Renderer retained its scene texture");
        require(deletedFramebuffers == 10, "Renderer did not release all framebuffers");
        require(deletedBuffers >= 6 && deletedVertexArrays >= 2, "Shared geometry or buffers leaked");
        require(deletedTextures >= 11, "Renderer or dummy texture leaked");
        const int total = deletedTextures + deletedBuffers + deletedFramebuffers + deletedVertexArrays;
        renderer.shutdown();
        require(total == deletedTextures + deletedBuffers + deletedFramebuffers + deletedVertexArrays,
            "Repeated shutdown deleted resources again");
        resources.shutdown();
        require(deletedPrograms == ShaderCatalog::programs.size(), "Resource manager did not release its shader programs");
        requireNoGlError();
        glfwDestroyWindow(window);
        glfwTerminate();
        renderer.shutdown(); // IDs are zero: safe even after context teardown.
    }
    require(lateDeletes == 0, "GL resource deletion occurred after context teardown");
    std::cout << "PASS: worker shutdown and renderer resource lifetime\n";
}

void checkEngine(bool run) {
    require(glfwInit() != 0, "GLFW initialization failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    {
        Engine engine;
        engine.init();
        observeDeletes();
        if (run) {
            glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
            engine.run();
        }
    }
    require(deletedFramebuffers == 10, "Engine did not release renderer resources");
    require(deletedPrograms == ShaderCatalog::programs.size(), "Engine did not release shader programs");
    require(lateDeletes == 0, "Engine destroyed the context before releasing GL resources");
    require(glfwGetCurrentContext() == nullptr, "Engine did not close the context");
}
}

void runRenderCorrectnessChecks();
void runRenderPerformanceChecks();
void runRenderStartupChecks();

int main() {
    try {
        checkKeyframes();
        checkLightCapacity<PointLightComponent>(RenderLimits::PointLights);
        checkLightCapacity<SpotLightComponent>(RenderLimits::SpotLights);
        checkLightCapacity<DirectionalLightComponent>(RenderLimits::DirectionalLights);
        std::cout << "PASS: keyframe boundaries and scene light limits\n";
        checkRenderer();
        checkEngine(true);
        checkEngine(false);
        std::cout << "PASS: Engine run and destructor cleanup\n";
        runRenderCorrectnessChecks();
        runRenderPerformanceChecks();
        runRenderStartupChecks();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
