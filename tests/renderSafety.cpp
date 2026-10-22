#include "app/engine.hpp"
#include "assets/model.hpp"
#include "assets/shaderCatalog.hpp"
#include "scene/gameObjectFactory.hpp"
#include "rendering/renderUtils.hpp"
#include "animation/assimpNode.hpp"
#include "animation/bone.hpp"
#include "scene/component.hpp"
#include "scene/scene.hpp"
#include "scene/selectionModel.hpp"
#include "rendering/renderLimits.hpp"
#include "rendering/opengl/shader.hpp"
#include <assimp/anim.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace togr::tests {

void runGuiChecks();

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

// Observe the actual positions uploaded by the viewer without changing GL state.
struct SkeletonDrawCapture {
    static SkeletonDrawCapture* active;
    PFNGLDRAWARRAYSPROC previous;
    std::vector<glm::vec3> points, lines;

    SkeletonDrawCapture() : previous(glad_glDrawArrays) {
        require(active == nullptr, "Skeleton draw captures must not overlap");
        active = this;
        glad_glDrawArrays = draw;
    }
    ~SkeletonDrawCapture() {
        glad_glDrawArrays = previous;
        active = nullptr;
    }
    SkeletonDrawCapture(const SkeletonDrawCapture&) = delete;
    SkeletonDrawCapture& operator=(const SkeletonDrawCapture&) = delete;
    static void APIENTRY draw(GLenum mode, GLint first, GLsizei count) {
        auto& capture = *active;
        if (mode == GL_POINTS || mode == GL_LINES) {
            GLint buffer = 0;
            glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
            auto& vertices = mode == GL_POINTS ? capture.points : capture.lines;
            const auto start = vertices.size();
            vertices.resize(start + static_cast<std::size_t>(count));
            glGetNamedBufferSubData(static_cast<GLuint>(buffer), first * sizeof(glm::vec3),
                count * sizeof(glm::vec3), vertices.data() + start);
        }
        capture.previous(mode, first, count);
    }
};
SkeletonDrawCapture* SkeletonDrawCapture::active = nullptr;

void checkKeyframes() {
    aiNodeAnim emptyChannel;
    const Bone empty(&emptyChannel);
    require(empty.sample(10.0f) == glm::mat4(1.0f), "Empty keyframes must be safe");

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
    const Bone bone(&channel);
    for (float time : { 1.0f, 2.0f, 100.0f }) {
        require(std::abs(bone.sample(time)[3].x - 4.0f) < 0.0001f, "Last keyframe must clamp safely");
    }
    require(std::abs(bone.sample(-1.0f)[3].x) < 0.0001f, "Before-first keyframe must clamp safely");
    require(std::abs(bone.sample(0.5f)[3].x - 2.0f) < 0.0001f, "Out-of-order immutable sampling changed interpolation");
    channel.mPositionKeys[1].mTime = channel.mRotationKeys[1].mTime = channel.mScalingKeys[1].mTime = 0;
    const Bone duplicateTimes(&channel);
    require(std::isfinite(duplicateTimes.sample(1.0f)[0].x), "Duplicate timestamps must stay finite");
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
    node.localBindTransform = node.modelToBoneBind = glm::mat4(1.0f);
    node.bindPosition = glm::vec3(0.0f);
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

    // Node positions use vector indices; bone matrices use the independent IDs.
    std::vector<Node> nodes = { makeNode(0, -1), makeNode(7, 0), makeNode(2, 999), makeNode(3, -2) };
    nodes[0].localBindTransform = glm::translate(glm::mat4(1.0f), glm::vec3(2, 0, 0));
    nodes[1].localBindTransform = glm::translate(glm::mat4(1.0f), glm::vec3(3, 0, 0));
    nodes[0].bindPosition = glm::vec3(2, 0, 0);
    nodes[1].bindPosition = glm::vec3(5, 0, 0);
    nodes[2].bindPosition = glm::vec3(7, 1, 0);
    nodes[3].bindPosition = glm::vec3(9, 2, 0);
    nodes[0].childrenIndices = { 1, -1, 999 };
    Animator animator(&animation, nodes);
    animator.updateAnimation(1.0f); // Zero duration, no animated channels.
    require(std::abs(animator.getPose().nodePositions[1].x - 5.0f) < 0.0001f, "Static ancestor transform was lost");
    require(std::abs(animator.getPose().boneMatrices[7][3].x - 5.0f) < 0.0001f,
        "Bone palette used the vector index instead of the bone ID");
    const auto sampledPose = animator.getPose();
    animator.getBoneMatrixTexture().use(GL_TEXTURE8);
    std::vector<glm::mat4> sampledPalette(TOGR_MAX_BONES);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, sampledPalette.data());

    auto shader = resources.getShader(ShaderCatalog::skeleton.key);
    shader->use();
    shader->setMat4("model", glm::mat4(1.0f));
    SkeletonPose displayPose = sampledPose;
    for (auto& position : displayPose.nodePositions) position += glm::vec3(0, 4, 0);
    const auto originalDisplayPose = displayPose;
    const std::vector<glm::vec3> bindPoints = {
        nodes[0].bindPosition, nodes[1].bindPosition, nodes[2].bindPosition, nodes[3].bindPosition};
    {
        SkeletonViewerComponent skeleton;
        {
            SkeletonDrawCapture captured;
            skeleton.drawSkeleton(nodes, &displayPose);
            require(captured.points.empty() && captured.lines.empty(), "Hidden viewer submitted skeleton geometry");
        }
        skeleton.show = true;
        {
            SkeletonDrawCapture captured;
            skeleton.drawSkeleton(nodes, &displayPose);
            require(captured.points == displayPose.nodePositions && captured.lines == std::vector<glm::vec3>{
                displayPose.nodePositions[1], displayPose.nodePositions[0]},
                "Viewer lost pose indexing or drew invalid parent connections");
        }
        SkeletonPose shortPose = displayPose;
        shortPose.nodePositions.resize(1);
        {
            SkeletonDrawCapture captured;
            skeleton.drawSkeleton(nodes, &shortPose);
            require(captured.points == bindPoints && captured.lines == std::vector<glm::vec3>{bindPoints[1], bindPoints[0]},
                "Short display pose did not fall back entirely to bind positions");
        }
        {
            SkeletonDrawCapture captured;
            skeleton.drawSkeleton(nodes); // Static display needs no animator.
            require(captured.points == bindPoints && captured.lines == std::vector<glm::vec3>{bindPoints[1], bindPoints[0]},
                "Static skeleton display requires an animator or lost bind positions");
        }
        skeleton.show = false;
        {
            SkeletonDrawCapture captured;
            skeleton.drawSkeleton(nodes, &displayPose);
            require(captured.points.empty() && captured.lines.empty(), "Viewer ignored a visibility change");
        }
    }
    require(animator.getPose().nodePositions == sampledPose.nodePositions &&
        animator.getPose().boneMatrices == sampledPose.boneMatrices &&
        displayPose.nodePositions == originalDisplayPose.nodePositions &&
        displayPose.boneMatrices == originalDisplayPose.boneMatrices,
        "Viewer visibility or lifetime changed a borrowed pose");
    animator.getBoneMatrixTexture().use(GL_TEXTURE8);
    std::vector<glm::mat4> finalPalette(TOGR_MAX_BONES);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, finalPalette.data());
    require(finalPalette == sampledPalette, "Viewer visibility or lifetime changed the animator's GPU palette");
    require(nodes[1].bindPosition == glm::vec3(5, 0, 0), "Animation changed shared bind positions");

    auto unanimatedModel = std::make_shared<Model>();
    std::weak_ptr<Model> modelLifetime = unanimatedModel;
    {
        AnimatorComponent noAnimations(unanimatedModel);
        require(noAnimations.isBoundTo(*unanimatedModel) && noAnimations.getAnimationsNames().empty(),
            "Model with no clips acquired an animation or lost its binding");
        unanimatedModel.reset();
        require(!modelLifetime.expired(), "AnimatorComponent did not retain its model owner");
        noAnimations.update(1.0f);
        require(noAnimations.getPose().nodePositions.empty(), "An empty model acquired pose nodes");
    }
    require(modelLifetime.expired(), "AnimatorComponent retained its model after destruction");
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
        runGuiChecks();
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

int runSafetyChecks() {
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

} // namespace togr::tests

int main() {
    return togr::tests::runSafetyChecks();
}
