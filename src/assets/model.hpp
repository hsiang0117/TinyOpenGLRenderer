#ifndef TOGR_MODEL_HPP
#define TOGR_MODEL_HPP
#pragma once

#include "rendering/material.hpp"
#include "rendering/geometry/mesh.hpp"
#include "animation/assimpNode.hpp"
#include "animation/animation.hpp"
#include <unordered_map>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct aiNode;
struct aiScene;
struct aiMesh;

namespace togr {

class Shader;

// importAsync() completes CPU import before its future becomes ready. Publishing
// and releasing GL resources require the owning thread's current GL context.
class Model {
public:
    Model() = default;

    // CPU import: copies the UTF-8 path before starting the worker; no GL calls.
    // A reported import failure yields a model that cannot be initialized.
    static std::future<std::shared_ptr<Model>> importAsync(const char* path);

    // GL publication: call on the owning thread with its context current.
    // False means CPU import failed, mesh upload failed, or GL was already initialized.
    // Optional material texture failures use defaults and do not fail publication.
    bool initGLResources();

    // Drawing requires isReady(), a valid shader, and the current GL context.
    void draw(std::shared_ptr<Shader> shader);

    // CPU queries; isReady() becomes true only after successful GL publication.
    std::string getPath() const { return path.u8string(); }
    std::string getName() const { return name; }
    bool isReady() const { return cpuImportSucceeded && glInitialized; }
    void buildAABB(glm::vec3& min, glm::vec3& max) const;

    // Imported hierarchy, bind data and clips stay read-only after import. The
    // Model must outlive these borrowed references; AnimatorComponent retains a
    // shared owner and keeps instance animation output in its own SkeletonPose.
    // findNode() returns nullptr when the requested name is absent.
    const Node* findNode(const std::string& name) const;
    const std::vector<Node>& getNodes() const { return nodes; }
    const std::vector<Animation>& getAnimations() const { return animations; }

private:
    // CPU import success does not imply that this model can be drawn. Only the
    // owning thread's successful GL publication makes isReady() true.
    bool cpuImportSucceeded = false;
    bool glInitialized = false;

    std::unordered_map<unsigned int, Material> materials;
    std::vector<MeshPtr> meshes;
    std::vector<Node> nodes;
    std::vector<Animation> animations;

    std::filesystem::path path;
    std::filesystem::path directory;
    std::string name;

    // CPU import flow: file -> hierarchy/meshes -> animation clips. Keep the
    // source traversal order: register node, visit children, then import meshes.
    void importFile(const std::filesystem::path& modelPath);
    void importNode(const aiNode& source, const aiScene& scene, int parentIndex);
    Mesh importMesh(const aiMesh& source, const aiScene& scene, const glm::mat4& meshToModelBind);
    void importAnimations(const aiScene& scene);

    // Nodes are identified by first appearance, including bone placeholders.
    // Return indices so no node reference survives vector growth or recursion.
    int importNodeHierarchy(const aiNode& source, int parentIndex);
    int findOrCreateNode(const std::string& name);

    // Mesh stages that update shared model state. Geometry conversion helpers
    // remain local to model.cpp; skin offsets consume baked model-space vertices.
    void importSkinBindings(const aiMesh& source, const glm::dmat4& modelToMesh, std::vector<Vertex>& vertices);
    int bindBoneNode(const std::string& name, const glm::mat4& modelOffset);
    void importMaterial(const aiScene& scene, unsigned int materialIndex);
};

using ModelPtr = std::shared_ptr<Model>;

} // namespace togr

#endif // !TOGR_MODEL_HPP
