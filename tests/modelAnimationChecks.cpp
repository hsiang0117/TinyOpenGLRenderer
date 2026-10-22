#include "assets/model.hpp"
#include "assets/shaderCatalog.hpp"
#include "scene/component.hpp"
#include "scene/gameObject.hpp"
#include "rendering/opengl/glBuffer.hpp"
#include "rendering/opengl/shader.hpp"
#include "rendering/renderUtils.hpp"
#include <assimp/Exporter.hpp>
#include <assimp/anim.h>
#include <assimp/cexport.h>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace togr::tests {

namespace {
constexpr float tolerance = 0.0005f;
const std::array<glm::vec3, 3> positions{{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}}};
const glm::vec3 sourceNormal = glm::normalize(glm::vec3(1, 2, 3));

void require(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
}

bool near(const glm::vec3& first, const glm::vec3& second) {
    for (int component = 0; component < 3; ++component)
        if (!std::isfinite(first[component]) || std::abs(first[component] - second[component]) > tolerance) return false;
    return true;
}

void requireMatrix(const glm::mat4& actual, const glm::mat4& expected, const char* message) {
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            require(std::isfinite(actual[column][row]) &&
                std::abs(actual[column][row] - expected[column][row]) <= tolerance, message);
}

aiMatrix4x4 assimpMatrix(const glm::mat4& value) {
    return aiMatrix4x4(value[0][0], value[1][0], value[2][0], value[3][0],
        value[0][1], value[1][1], value[2][1], value[3][1],
        value[0][2], value[1][2], value[2][2], value[3][2],
        value[0][3], value[1][3], value[2][3], value[3][3]);
}

enum class FixtureKind { Valid, SingularMesh, ConflictingBone };

// Own client-allocated data here. The exported aiScene/aiNode destructors live
// in the bundled DLL, whose CRT differs from the Release test executable.
// Detach their borrowed arrays before destroying either object.
struct Fixture {
    aiScene scene;
    std::vector<std::unique_ptr<aiNode>> nodes;
    std::vector<aiNode*> children;
    std::vector<std::unique_ptr<aiMesh>> meshes;
    std::vector<aiMesh*> meshPointers;
    std::unique_ptr<aiMaterial> material = std::make_unique<aiMaterial>();
    std::unique_ptr<aiAnimation> animation = std::make_unique<aiAnimation>();
    aiMaterial* materialPointer[1] = {material.get()};
    aiAnimation* animationPointer[1] = {animation.get()};
    unsigned int meshIndices[3] = {0, 1, 2};
    glm::mat4 root = glm::translate(glm::mat4(1), glm::vec3(3, -2, 5)) *
        glm::rotate(glm::mat4(1), glm::radians(90.0f), glm::vec3(0, 0, 1)) *
        glm::scale(glm::mat4(1), glm::vec3(2, 3, 4));
    std::array<glm::mat4, 3> meshTransforms;
    glm::mat4 boneBind = root * glm::translate(glm::mat4(1), glm::vec3(0, 1, 0));

    explicit Fixture(FixtureKind kind, bool boneFirst = false) {
        const auto first = glm::translate(glm::mat4(1), glm::vec3(1, 2, 0)) *
            glm::rotate(glm::mat4(1), glm::radians(90.0f), glm::vec3(0, 0, 1)) *
            glm::scale(glm::mat4(1), glm::vec3(1, 2, 1));
        const auto second = glm::translate(glm::mat4(1), glm::vec3(-2, 1, 1)) *
            glm::rotate(glm::mat4(1), glm::radians(-90.0f), glm::vec3(1, 0, 0)) *
            glm::scale(glm::mat4(1), glm::vec3(2, 1, 3));
        meshTransforms = {root * first, root * second, root * first};
        if (kind == FixtureKind::SingularMesh)
            meshTransforms[0] *= glm::scale(glm::mat4(1), glm::vec3(0, 1, 1));
        const char* names[] = {"FixtureRoot", "SkinnedFirst", "SkinnedSecond", "StaticControl", "SharedBone"};
        for (const auto name : names) {
            auto node = std::make_unique<aiNode>();
            node->mName = aiString(name);
            nodes.push_back(std::move(node));
        }
        nodes[0]->mTransformation = assimpMatrix(root);
        for (int index = 0; index < 3; ++index) {
            auto& node = *nodes[index + 1];
            node.mParent = nodes[0].get();
            node.mTransformation = assimpMatrix(glm::inverse(root) * meshTransforms[index]);
            node.mNumMeshes = 1;
            node.mMeshes = &meshIndices[index];
        }
        nodes[4]->mParent = nodes[0].get();
        nodes[4]->mTransformation = assimpMatrix(glm::translate(glm::mat4(1), glm::vec3(0, 1, 0)));
        for (std::size_t index = 1; index < nodes.size(); ++index) children.push_back(nodes[index].get());
        // Change traversal only; mesh indices, transforms and node storage stay fixed.
        if (boneFirst) {
            std::rotate(children.begin(), children.end() - 1, children.end());
        }
        nodes[0]->mNumChildren = static_cast<unsigned int>(children.size());
        nodes[0]->mChildren = children.data();
        scene.mRootNode = nodes[0].get();

        for (int index = 0; index < 3; ++index) {
            auto mesh = std::make_unique<aiMesh>();
            mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
            mesh->mNumVertices = 3;
            mesh->mVertices = new aiVector3D[3];
            mesh->mNormals = new aiVector3D[3];
            mesh->mTextureCoords[0] = new aiVector3D[3];
            mesh->mNumUVComponents[0] = 2;
            for (int vertex = 0; vertex < 3; ++vertex) {
                const auto& point = positions[vertex];
                mesh->mVertices[vertex] = aiVector3D(point.x, point.y, point.z);
                mesh->mNormals[vertex] = aiVector3D(sourceNormal.x, sourceNormal.y, sourceNormal.z);
                mesh->mTextureCoords[0][vertex] = aiVector3D(point.x, point.y, 0);
            }
            mesh->mNumFaces = 1;
            mesh->mFaces = new aiFace[1];
            mesh->mFaces[0].mNumIndices = 3;
            mesh->mFaces[0].mIndices = new unsigned int[3]{0, 1, 2};
            if (index < 2) {
                mesh->mNumBones = 1;
                mesh->mBones = new aiBone*[1]{new aiBone()};
                auto& bone = *mesh->mBones[0];
                bone.mName = aiString("SharedBone");
                auto offset = glm::inverse(boneBind) * meshTransforms[index];
                if (kind == FixtureKind::ConflictingBone && index == 1) offset[3].x += 0.5f;
                bone.mOffsetMatrix = assimpMatrix(offset);
                bone.mNumWeights = 3;
                bone.mWeights = new aiVertexWeight[3]{{0, 1}, {1, 1}, {2, 1}};
            }
            meshPointers.push_back(mesh.get());
            meshes.push_back(std::move(mesh));
        }
        scene.mNumMeshes = static_cast<unsigned int>(meshPointers.size());
        scene.mMeshes = meshPointers.data();
        scene.mNumMaterials = 1;
        scene.mMaterials = materialPointer;

        animation->mName = aiString("MoveBone");
        animation->mDuration = 2;
        animation->mTicksPerSecond = 1;
        animation->mNumChannels = 1;
        animation->mChannels = new aiNodeAnim*[1]{new aiNodeAnim()};
        auto& channel = *animation->mChannels[0];
        channel.mNodeName = aiString("SharedBone");
        channel.mNumPositionKeys = 2;
        channel.mPositionKeys = new aiVectorKey[2];
        channel.mPositionKeys[0].mTime = 0;
        channel.mPositionKeys[0].mValue = aiVector3D(0, 1, 0);
        channel.mPositionKeys[1].mTime = 2;
        channel.mPositionKeys[1].mValue = aiVector3D(4, 1, 0);
        channel.mNumRotationKeys = channel.mNumScalingKeys = 1;
        channel.mRotationKeys = new aiQuatKey[1];
        channel.mRotationKeys[0].mValue = aiQuaternion(1, 0, 0, 0);
        channel.mScalingKeys = new aiVectorKey[1];
        channel.mScalingKeys[0].mValue = aiVector3D(1, 1, 1);
        scene.mNumAnimations = 1;
        scene.mAnimations = animationPointer;
    }

    ~Fixture() {
        scene.mRootNode = nullptr;
        scene.mNumMeshes = scene.mNumMaterials = scene.mNumAnimations = 0;
        scene.mMeshes = nullptr;
        scene.mMaterials = nullptr;
        scene.mAnimations = nullptr;
        for (auto& node : nodes) {
            node->mNumChildren = node->mNumMeshes = 0;
            node->mChildren = nullptr;
            node->mMeshes = nullptr;
        }
    }
};

void writeFixture(const Fixture& fixture, const std::filesystem::path& path) {
    Assimp::Exporter exporter;
    const auto blob = exporter.ExportToBlob(&fixture.scene, "assbin");
    require(blob && blob->data && blob->size && !blob->next, "Assimp could not export the animation fixture to one assbin blob");
    std::ofstream output(path, std::ios::binary);
    output.write(static_cast<const char*>(blob->data), static_cast<std::streamsize>(blob->size));
    require(bool(output), "Animation fixture write failed");
}

std::vector<glm::mat4> palette(Texture2D& texture) {
    texture.use(GL_TEXTURE8);
    std::vector<glm::mat4> matrices(TOGR_MAX_BONES);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, matrices.data());
    return matrices;
}

std::vector<glm::mat4> palette(Animator& animator) {
    return palette(animator.getBoneMatrixTexture());
}

struct CapturedVertex {
    glm::vec4 position;
    glm::vec3 normal;
};

struct CaptureState {
    GLint program, vertexArray, uniformBuffer, uniformBinding, feedbackBuffer, feedbackBinding;
    GLboolean discard;
    CaptureState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniformBuffer);
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &uniformBinding);
        glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &feedbackBuffer);
        glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &feedbackBinding);
        discard = glIsEnabled(GL_RASTERIZER_DISCARD);
    }
    ~CaptureState() {
        glUseProgram(program);
        glBindVertexArray(vertexArray);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, uniformBinding);
        glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, feedbackBinding);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, feedbackBuffer);
        if (!discard) glDisable(GL_RASTERIZER_DISCARD);
    }
};

std::vector<CapturedVertex> capture(Model& model, Animator& animator, const char* vertexPath,
    const std::filesystem::path& fragmentPath, bool withNormals,
    const GameObjectPtr& object = nullptr, GLuint expectedTriangles = 3) {
    CaptureState state;
    auto shader = std::make_shared<Shader>(vertexPath, fragmentPath.u8string().c_str());
    require(shader->isValid(), "Production skinning vertex shader did not compile");
    const char* varyings[] = {"gl_Position", "VS_OUT.normal"};
    glTransformFeedbackVaryings(shader->ID, withNormals ? 2 : 1, varyings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(shader->ID);
    GLint linked = GL_FALSE;
    glGetProgramiv(shader->ID, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        char log[2048]{};
        glGetProgramInfoLog(shader->ID, sizeof(log), nullptr, log);
        std::cerr << vertexPath << ": " << log << '\n';
    }
    require(linked == GL_TRUE, "Production skinning transform feedback did not link");
    shader->use();
    shader->setMat4("model", glm::mat4(1));
    shader->setMat4("lightMatrices", glm::mat4(1));
    shader->setMat4("lightSpaceMatrix", glm::mat4(1));
    UniformBuffer matrices;
    matrices.init(); matrices.bind(); matrices.bufferBase(0);
    const glm::mat4 viewProjection[] = {glm::mat4(1), glm::mat4(1)};
    matrices.bufferData(sizeof(viewProjection), viewProjection);
    struct FeedbackObjects {
        GLuint buffer = 0, query = 0;
        ~FeedbackObjects() { glDeleteBuffers(1, &buffer); glDeleteQueries(1, &query); }
    } objects;
    glGenBuffers(1, &objects.buffer);
    glGenQueries(1, &objects.query);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, objects.buffer);
    const int stride = withNormals ? 7 : 4;
    std::vector<float> data(9 * stride);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, data.size() * sizeof(float), nullptr, GL_STREAM_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, objects.buffer);
    animator.getBoneMatrixTexture().use(GL_TEXTURE8);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, objects.query);
    glBeginTransformFeedback(GL_TRIANGLES);
    if (object) {
        RenderHelpers::drawRenderable(object, shader);
    } else {
        model.draw(shader);
    }
    glEndTransformFeedback();
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    GLuint triangleCount = 0;
    glGetQueryObjectuiv(objects.query, GL_QUERY_RESULT, &triangleCount);
    require(triangleCount == expectedTriangles, "Model submission drew an unexpected number of fixture triangles");
    if (expectedTriangles == 0) return {};
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, objects.buffer);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, data.size() * sizeof(float), data.data());
    std::vector<CapturedVertex> result;
    for (int vertex = 0; vertex < 9; ++vertex) {
        const auto offset = vertex * stride;
        result.push_back({glm::vec4(data[offset], data[offset + 1], data[offset + 2], data[offset + 3]),
            withNormals ? glm::vec3(data[offset + 4], data[offset + 5], data[offset + 6]) : glm::vec3(0)});
    }
    return result;
}

std::vector<CapturedVertex> expectedVertices(const Fixture& fixture, const glm::vec3& displacement) {
    std::vector<CapturedVertex> result;
    for (int mesh = 0; mesh < 3; ++mesh) {
        const auto normal = glm::normalize(glm::transpose(glm::inverse(glm::mat3(fixture.meshTransforms[mesh]))) * sourceNormal);
        for (const auto& position : positions) {
            auto transformed = fixture.meshTransforms[mesh] * glm::vec4(position, 1);
            if (mesh < 2) transformed += glm::vec4(displacement, 0);
            result.push_back({transformed, normal});
        }
    }
    return result;
}

void requireVertices(const std::vector<CapturedVertex>& actual, const std::vector<CapturedVertex>& expected, bool withNormals) {
    std::vector<bool> matched(expected.size(), false);
    for (const auto& vertex : actual) {
        std::size_t index = 0;
        for (; index < expected.size(); ++index) {
            if (!matched[index] && near(glm::vec3(vertex.position), glm::vec3(expected[index].position)) &&
                (!withNormals || near(vertex.normal, expected[index].normal))) break;
        }
        require(index < expected.size(), "GPU skinning applied a root/mesh transform twice, lost a shared offset, or transformed a normal incorrectly");
        require(std::isfinite(vertex.position.w) && std::abs(vertex.position.w - 1) <= tolerance,
            "GPU skinning changed the homogeneous vertex weight");
        matched[index] = true;
    }
}

void requirePose(const SkeletonPose& actual, const SkeletonPose& expected, const char* message) {
    require(actual.nodePositions.size() == expected.nodePositions.size() &&
        actual.boneMatrices.size() == expected.boneMatrices.size(), message);
    for (std::size_t index = 0; index < actual.nodePositions.size(); ++index) {
        require(near(actual.nodePositions[index], expected.nodePositions[index]), message);
    }
    for (std::size_t index = 0; index < actual.boneMatrices.size(); ++index) {
        requireMatrix(actual.boneMatrices[index], expected.boneMatrices[index], message);
    }
}

void requireAnimatorUnchanged(Animator& animator, const SkeletonPose& expected,
    const std::vector<glm::mat4>& expectedPalette, const char* message) {
    requirePose(animator.getPose(), expected, message);
    const auto uploaded = palette(animator);
    require(uploaded.size() == expectedPalette.size(), message);
    for (std::size_t index = 0; index < uploaded.size(); ++index) {
        requireMatrix(uploaded[index], expectedPalette[index], message);
    }
}

void checkImportedAnimation(const std::filesystem::path& directory,
    const std::filesystem::path& fragmentPath, bool boneFirst) {
    Fixture fixture(FixtureKind::Valid, boneFirst);
    const auto modelPath = directory / (boneFirst ? "transformed-bone-first.assbin" : "transformed-mesh-first.assbin");
    writeFixture(fixture, modelPath);
    auto model = Model::importAsync(modelPath.u8string().c_str()).get();
    require(model->initGLResources() && model->isReady(), "Animation fixture import failed");
    require(model->getAnimations().size() == 1, "Assbin fixture lost its animation");
    const auto& nodes = model->getNodes();
    const auto bindNodes = nodes;
    const auto& clip = model->getAnimations().front();
    const auto clipName = clip.getName();
    const auto clipDuration = clip.getDuration();
    const auto clipRate = clip.getTicksPerSecond();
    const auto* track = clip.findBone("SharedBone");
    require(track != nullptr, "Assbin fixture lost its animation track");
    const std::array<float, 3> sampleTicks{{0.0f, 0.5f, 1.5f}};
    const std::array<glm::mat4, 3> originalSamples{{
        track->sample(sampleTicks[0]), track->sample(sampleTicks[1]), track->sample(sampleTicks[2])}};
    require(nodes.size() == 5, "Node traversal duplicated or discarded a hierarchy node");
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        require(nodes[index].id == static_cast<int>(index), "Imported node ID no longer matches its vector index");
    }
    require(std::count_if(nodes.begin(), nodes.end(), [](const Node& node) { return node.name == "SharedBone"; }) == 1,
        "A bone placeholder was not reused when its hierarchy node was imported");
    auto bone = model->findNode("SharedBone");
    require(bone && bone->isBoneNode && bone->id >= 0 && bone->id < TOGR_MAX_BONES, "Assbin fixture lost its shared bone");
    require(bone->id == (boneFirst ? 1 : 2), "Bone ID changed for the fixture's node traversal order");
    const auto boneId = bone->id;
    const auto root = model->findNode("FixtureRoot");
    require(root && root->id == 0 && root->parentIndex == -1, "Imported root identity or parent changed");
    require(root->childrenIndices.size() == 4 && bone->parentIndex == root->id && bone->childrenIndices.empty(),
        "Bone hierarchy completion lost its parent or changed the fixture topology");
    for (const auto child : root->childrenIndices) {
        require(child >= 0 && static_cast<std::size_t>(child) < nodes.size() && nodes[child].parentIndex == root->id,
            "Imported child link does not agree with its parent index");
        require(std::count(root->childrenIndices.begin(), root->childrenIndices.end(), child) == 1,
            "Imported parent contains a duplicate child link");
    }
    if (!near(bone->bindPosition, glm::vec3(fixture.boneBind[3]))) {
        auto printMatrix = [](const char* name, const glm::mat4& matrix) {
            std::cerr << name << '\n';
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) std::cerr << matrix[column][row] << ' ';
                std::cerr << '\n';
            }
        };
        std::cerr << "Imported SharedBone position: " << bone->bindPosition.x << ' ' << bone->bindPosition.y << ' ' << bone->bindPosition.z << '\n'
            << "Expected SharedBone position: " << fixture.boneBind[3].x << ' ' << fixture.boneBind[3].y << ' ' << fixture.boneBind[3].z << '\n'
            << "Imported SharedBone id/parent: " << bone->id << ' ' << bone->parentIndex << '\n';
        printMatrix("Fixture root:", fixture.root);
        printMatrix("Fixture boneBind:", fixture.boneBind);
        if (const auto root = model->findNode("FixtureRoot")) {
            printMatrix("Imported root local:", root->localBindTransform);
            std::cerr << "Imported root id/parent: " << root->id << ' ' << root->parentIndex << '\n';
        }
        printMatrix("Imported SharedBone local:", bone->localBindTransform);
    }
    require(near(bone->bindPosition, glm::vec3(fixture.boneBind[3])), "Imported skeleton is not in the baked mesh's model space");
    auto expectedBind = expectedVertices(fixture, glm::vec3(0));
    auto expectedMin = glm::vec3(expectedBind.front().position), expectedMax = expectedMin;
    for (const auto& vertex : expectedBind) {
        expectedMin = glm::min(expectedMin, glm::vec3(vertex.position));
        expectedMax = glm::max(expectedMax, glm::vec3(vertex.position));
    }
    glm::vec3 minimum, maximum;
    model->buildAABB(minimum, maximum);
    require(near(minimum, expectedMin) && near(maximum, expectedMax), "Bind AABB disagrees with the imported mesh/root transforms");
    // Animation runs without any SkeletonViewerComponent owning its pose.
    Animator animator(&clip, nodes);
    animator.updateAnimation(0);
    require(animator.getPose().nodePositions.size() == nodes.size(), "Animator pose lost node-vector indexing");
    require(near(animator.getPose().nodePositions[boneId], glm::vec3(fixture.boneBind[3])),
        "Animator bind pose disagrees with the imported bone origin");
    requireMatrix(palette(animator)[boneId], glm::mat4(1), "Bind pose palette must be identity for vertices already baked into model space");
    const char* stages[] = {ShaderCatalog::forward.vertexPath, ShaderCatalog::directionalShadow.vertexPath, ShaderCatalog::pointShadow.vertexPath};
    for (int stage = 0; stage < 3; ++stage)
        requireVertices(capture(*model, animator, stages[stage], fragmentPath, stage == 0), expectedBind, stage == 0);

    animator.updateAnimation(0.5f);
    const auto animatedBone = fixture.root * glm::translate(glm::mat4(1), glm::vec3(1, 1, 0));
    require(near(animator.getPose().nodePositions[boneId], glm::vec3(animatedBone[3])), "Animated skeleton lost the nonidentity root transform");
    const auto skinTransform = animatedBone * glm::inverse(fixture.boneBind);
    requireMatrix(palette(animator)[boneId], skinTransform, "Animated palette does not match the shared bone's model-space displacement");
    const auto expectedAnimated = expectedVertices(fixture, glm::vec3(0, 2, 0));
    for (int stage = 0; stage < 3; ++stage)
        requireVertices(capture(*model, animator, stages[stage], fragmentPath, stage == 0), expectedAnimated, stage == 0);

    auto requireAtTime = [&](Animator& instance, float seconds) {
        const auto expectedBone = fixture.root * glm::translate(glm::mat4(1), glm::vec3(2 * seconds, 1, 0));
        const auto& pose = instance.getPose();
        require(pose.nodePositions.size() == nodes.size(), "Animator pose lost its node positions");
        require(near(pose.nodePositions[boneId], glm::vec3(expectedBone[3])), "Independent animator sampled the wrong time");
        requireMatrix(pose.boneMatrices[boneId], expectedBone * glm::inverse(fixture.boneBind),
            "Independent CPU palette does not match the fixture displacement");
        const auto uploaded = palette(instance);
        require(uploaded.size() == pose.boneMatrices.size(), "CPU and GPU palette sizes disagree");
        for (std::size_t index = 0; index < uploaded.size(); ++index) {
            requireMatrix(uploaded[index], pose.boneMatrices[index], "GPU palette does not match its animator's CPU pose");
        }
    };
    requireAtTime(animator, 0.5f);
    const auto firstPose = animator.getPose();
    const auto firstPalette = palette(animator);
    Animator second(&clip, nodes);
    second.updateAnimation(1.0f);
    requireAtTime(second, 1.0f);
    requireAnimatorUnchanged(animator, firstPose, firstPalette, "Updating the second animator changed the first instance");
    requireVertices(capture(*model, second, stages[0], fragmentPath, true),
        expectedVertices(fixture, glm::vec3(0, 4, 0)), true);
    const auto secondPose = second.getPose();
    const auto secondPalette = palette(second);

    animator.updateAnimation(0.25f);
    requireAtTime(animator, 0.75f);
    requireAnimatorUnchanged(second, secondPose, secondPalette, "Updating the first animator changed the second instance");
    const auto advancedFirstPose = animator.getPose();
    const auto advancedFirstPalette = palette(animator);
    second.updateAnimation(0.25f);
    requireAtTime(second, 1.25f);
    requireAnimatorUnchanged(animator, advancedFirstPose, advancedFirstPalette,
        "Interleaved sampling changed an animator that was not updated");

    {
        AnimatorComponent independent(model);
        const auto otherModel = std::make_shared<Model>();
        independent.playAnimation(clipName);
        independent.update(0.5f);
        require(independent.isBoundTo(*model) && !independent.isBoundTo(*otherModel) &&
            near(independent.getPose().nodePositions[boneId], glm::vec3(animatedBone[3])),
            "AnimatorComponent cannot animate its model independently of a skeleton viewer");
        const auto selectedPose = independent.getPose();
        independent.playAnimation(clipName + " missing");
        require(independent.getCurrentAnimation() == clipName, "Unknown clip name replaced the actual selected animation");
        requirePose(independent.getPose(), selectedPose, "Unknown clip selection changed the sampled pose");
        independent.update(0.25f);
        const auto threeQuarterBone = fixture.root * glm::translate(glm::mat4(1), glm::vec3(1.5f, 1, 0));
        require(near(independent.getPose().nodePositions[boneId], glm::vec3(threeQuarterBone[3])),
            "Unknown clip selection reset the running clock");
        const auto pausedPose = independent.getPose();
        const auto pausedPalette = palette(independent.getBoneMatrixTexture());
        independent.playAnimation(clipName);
        requirePose(independent.getPose(), pausedPose, "Selecting a clip sampled it before update");
        const auto stillPausedPalette = palette(independent.getBoneMatrixTexture());
        for (std::size_t index = 0; index < pausedPalette.size(); ++index) {
            requireMatrix(stillPausedPalette[index], pausedPalette[index], "Clip selection prematurely changed the GPU palette");
        }
        independent.update(0);
        require(near(independent.getPose().nodePositions[boneId], glm::vec3(fixture.boneBind[3])),
            "Zero-time update did not sample the newly selected clip's first frame");
        requireMatrix(palette(independent.getBoneMatrixTexture())[boneId], glm::mat4(1),
            "Restarting a clip did not reset the GPU bind palette");

        auto mismatched = std::make_shared<GameObject>("Mismatched model and animator");
        mismatched->addComponent<Transform>();
        mismatched->addComponent<ModelRenderComponent>()->setModel(model);
        mismatched->addComponent<AnimatorComponent>(otherModel);
        capture(*model, animator, stages[0], fragmentPath, true, mismatched, 0);
    }
    require(nodes.size() == bindNodes.size(), "Animation changed the model's bind node count");
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const auto& actual = nodes[index];
        const auto& expected = bindNodes[index];
        require(actual.name == expected.name && actual.id == expected.id && actual.parentIndex == expected.parentIndex &&
            actual.bindPosition == expected.bindPosition && actual.localBindTransform == expected.localBindTransform &&
            actual.modelToBoneBind == expected.modelToBoneBind && actual.isBoneNode == expected.isBoneNode &&
            actual.childrenIndices == expected.childrenIndices, "Instance animation mutated shared model bind data");
    }
    require(clip.isValid() && clip.getName() == clipName && clip.getDuration() == clipDuration &&
        clip.getTicksPerSecond() == clipRate && model->getAnimations().size() == 1,
        "Instance animation changed the shared clip metadata");
    for (std::size_t index = 0; index < sampleTicks.size(); ++index) {
        requireMatrix(track->sample(sampleTicks[index]), originalSamples[index], "Interleaved animation changed shared key tracks");
    }
    model->buildAABB(minimum, maximum);
    require(near(minimum, expectedMin) && near(maximum, expectedMax), "Animation unexpectedly changed the model's bind-pose AABB");
}
}

void runModelAnimationChecks() {
    const std::filesystem::path directory = "x64/RenderChecks/model-animation";
    std::filesystem::create_directories(directory);
    const auto fragmentPath = directory / "feedback.frag";
    {
        std::ofstream output(fragmentPath);
        output << "#version 450 core\nout vec4 color;\nvoid main(){color=vec4(1);}\n";
        require(bool(output), "Animation feedback shader write failed");
    }
    for (const bool boneFirst : {false, true}) {
        checkImportedAnimation(directory, fragmentPath, boneFirst);
    }
    for (const auto kind : {FixtureKind::SingularMesh, FixtureKind::ConflictingBone}) {
        Fixture invalidFixture(kind);
        const auto invalidPath = directory / (kind == FixtureKind::SingularMesh ? "singular.assbin" : "conflicting.assbin");
        writeFixture(invalidFixture, invalidPath);
        auto invalid = Model::importAsync(invalidPath.u8string().c_str()).get();
        require(!invalid->initGLResources() && !invalid->isReady(), "Unsafe skinning transforms must fail model loading");
    }
    std::cout << "PASS: both bone/mesh traversal orders, stable bind hierarchy, independent CPU/GPU animator poses without viewers, immutable model/clips, three GPU skinning stages, and invalid rigs\n";
}

} // namespace togr::tests
