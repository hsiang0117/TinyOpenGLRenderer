#include "model.hpp"
#include "rendering/opengl/shader.hpp"
#include "assimpGlmHelpers.hpp"
#include "assimpFileSystem.hpp"
#include "core/logger.hpp"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace togr {

namespace {
// Vertices retain the original float transform. Inversion and direction
// calculations use double precision, then convert their final results to float.
struct MeshBindTransforms {
    glm::mat4 meshToModel;
    glm::dmat4 modelToMesh;
    glm::dmat3 directionToModel;
    glm::dmat3 normalToModel;
};

glm::mat4 meshToModelBindTransform(const aiNode& source);
MeshBindTransforms makeMeshBindTransforms(const aiMesh& source, const glm::mat4& meshToModelBind);
std::vector<Vertex> importVertices(const aiMesh& source, const MeshBindTransforms& bind);
std::vector<GLuint> importIndices(const aiMesh& source);
void importBoneWeights(const aiBone& source, int boneId, std::vector<Vertex>& vertices);
Material importMaterialPaths(const aiMaterial& source, const std::filesystem::path& modelDirectory);
bool finiteMatrix(const glm::dmat4& matrix);
bool compatibleOffsets(const glm::mat4& first, const glm::mat4& second);
}

std::future<std::shared_ptr<Model>> Model::importAsync(const char* path) {
    const auto modelPath = std::filesystem::u8path(path ? path : "");
    return std::async(std::launch::async, [modelPath]() {
        auto model = std::make_shared<Model>();
        model->importFile(modelPath);
        return model;
    });
}

bool Model::initGLResources() {
    if (!cpuImportSucceeded || glInitialized) return false;

    for (auto& mesh : meshes) {
        if (!mesh->initGLResources()) {
            TOGR_LOG_ERROR("Failed to initialize mesh GL resources");
            return false;
        }
    }
    for (auto& entry : materials) {
        if (!entry.second.initGLResources()) {
            TOGR_LOG_WARN("Model uses defaults for failed material textures: " << path.u8string()
                << " (material " << entry.first << ")");
        }
    }
    glInitialized = true;
    return true;
}

void Model::draw(ShaderPtr shader) {
    for (const auto& mesh : meshes) {
        auto material = materials.find(mesh->getMaterialIndex());
        if (material != materials.end()) {
            material->second.bind(shader);
        }
        mesh->draw();
    }
}

void Model::buildAABB(glm::vec3& min, glm::vec3& max) const {
    min = glm::vec3(std::numeric_limits<float>::max());
    max = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& mesh : meshes) {
        if (!mesh) continue;
        for (const auto& vertex : mesh->vertices) {
            min.x = std::min(min.x, vertex.position.x);
            min.y = std::min(min.y, vertex.position.y);
            min.z = std::min(min.z, vertex.position.z);
            max.x = std::max(max.x, vertex.position.x);
            max.y = std::max(max.y, vertex.position.y);
            max.z = std::max(max.z, vertex.position.z);
        }
    }
}

const Node* Model::findNode(const std::string& name) const {
    for (const auto& node : nodes) {
        if (node.name == name) return &node;
    }
    return nullptr;
}

void Model::importFile(const std::filesystem::path& modelPath) {
    Assimp::Importer importer;
    importer.SetIOHandler(new AssimpFileSystem());
    const aiScene* scene = importer.ReadFile(modelPath.u8string(), aiProcess_Triangulate | aiProcess_CalcTangentSpace);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        TOGR_LOG_ERROR("ERROR::ASSIMP::" << importer.GetErrorString());
        return;
    }

    path = modelPath;
    directory = modelPath.parent_path();
    name = modelPath.stem().u8string();
    try {
        importNode(*scene->mRootNode, *scene, -1);
        importAnimations(*scene);
    } catch (const std::runtime_error& error) {
        TOGR_LOG_ERROR("Cannot import model " << modelPath.u8string() << ": " << error.what());
        meshes.clear();
        materials.clear();
        nodes.clear();
        animations.clear();
        return;
    }
    importer.FreeScene();
    cpuImportSucceeded = true;
}

void Model::importNode(const aiNode& source, const aiScene& scene, int parentIndex) {
    const int nodeIndex = importNodeHierarchy(source, parentIndex);

    // Preserve children-before-mesh traversal. A mesh can introduce a bone
    // placeholder before its actual hierarchy node appears in a later sibling.
    for (unsigned int child = 0; child < source.mNumChildren; ++child) {
        importNode(*source.mChildren[child], scene, nodeIndex);
    }
    for (unsigned int mesh = 0; mesh < source.mNumMeshes; ++mesh) {
        const auto meshToModelBind = meshToModelBindTransform(source);
        const aiMesh& sourceMesh = *scene.mMeshes[source.mMeshes[mesh]];
        meshes.push_back(std::make_shared<Mesh>(importMesh(sourceMesh, scene, meshToModelBind)));
    }
}

Mesh Model::importMesh(const aiMesh& source, const aiScene& scene, const glm::mat4& meshToModelBind) {
    const auto bind = makeMeshBindTransforms(source, meshToModelBind);
    Mesh result;
    result.vertices = importVertices(source, bind);
    importSkinBindings(source, bind.modelToMesh, result.vertices);
    result.indices = importIndices(source);
    result.setMaterialIndex(source.mMaterialIndex);
    importMaterial(scene, source.mMaterialIndex);
    return result;
}

void Model::importAnimations(const aiScene& scene) {
    for (unsigned int animation = 0; animation < scene.mNumAnimations; ++animation) {
        animations.emplace_back(scene.mAnimations[animation], nodes);
    }
}

int Model::importNodeHierarchy(const aiNode& source, int parentIndex) {
    const int nodeIndex = findOrCreateNode(source.mName.C_Str());
    Node& node = nodes[nodeIndex];
    node.parentIndex = parentIndex;
    if (parentIndex != -1) {
        nodes[parentIndex].childrenIndices.push_back(nodeIndex);
    }
    node.localBindTransform = AssimpGLMHelpers::ConvertMatrixToGLMFormat(source.mTransformation);
    node.bindPosition = node.localBindTransform * glm::vec4(0, 0, 0, 1);
    for (int ancestor = parentIndex; ancestor != -1; ancestor = nodes[ancestor].parentIndex) {
        node.bindPosition = nodes[ancestor].localBindTransform * glm::vec4(node.bindPosition, 1.0f);
    }
    // Completing a placeholder must retain its existing ID and skin binding.
    return nodeIndex;
}

int Model::findOrCreateNode(const std::string& name) {
    if (const auto* node = findNode(name)) return node->id;

    Node node;
    node.name = name;
    node.id = static_cast<int>(nodes.size());
    node.parentIndex = -1;
    node.bindPosition = glm::vec3(0.0f);
    node.localBindTransform = glm::mat4(1.0f);
    node.modelToBoneBind = glm::mat4(1.0f);
    const int nodeIndex = node.id;
    nodes.push_back(std::move(node));
    return nodeIndex;
}

void Model::importSkinBindings(const aiMesh& source, const glm::dmat4& modelToMesh, std::vector<Vertex>& vertices) {
    for (unsigned int bone = 0; bone < source.mNumBones; ++bone) {
        const aiBone& sourceBone = *source.mBones[bone];
        const std::string boneName = sourceBone.mName.C_Str();
        const glm::dmat4 meshOffset(AssimpGLMHelpers::ConvertMatrixToGLMFormat(sourceBone.mOffsetMatrix));
        // q = M * p is already in model space. O_model = O * inverse(M), so
        // G(t) * O_model * q equals G(t) * O * p without applying M twice.
        const glm::mat4 modelOffset(meshOffset * modelToMesh);
        if (!finiteMatrix(meshOffset) || !finiteMatrix(glm::dmat4(modelOffset))) {
            throw std::runtime_error("Non-finite skin binding for bone: " + boneName);
        }
        const int boneId = bindBoneNode(boneName, modelOffset);
        if (boneId < 0 || boneId >= TOGR_MAX_BONES) {
            TOGR_LOG_WARN("Skipping bone outside the GPU bone capacity: " << boneName);
            continue;
        }
        importBoneWeights(sourceBone, boneId, vertices);
    }
}

int Model::bindBoneNode(const std::string& name, const glm::mat4& modelOffset) {
    const int nodeIndex = findOrCreateNode(name);
    Node& node = nodes[nodeIndex];
    if (node.isBoneNode && !compatibleOffsets(node.modelToBoneBind, modelOffset)) {
        throw std::runtime_error("Unsupported incompatible mesh bindings for shared bone: " + name);
    }
    if (!node.isBoneNode) {
        node.modelToBoneBind = modelOffset;
    }
    node.isBoneNode = true;
    return node.id;
}

void Model::importMaterial(const aiScene& scene, unsigned int materialIndex) {
    if (materials.find(materialIndex) != materials.end()) return;
    materials.emplace(materialIndex, importMaterialPaths(*scene.mMaterials[materialIndex], directory));
}

namespace {
glm::mat4 meshToModelBindTransform(const aiNode& source) {
    auto meshToModel = AssimpGLMHelpers::ConvertMatrixToGLMFormat(source.mTransformation);
    // Keep the leaf-to-root left-multiplication order of the Assimp node chain.
    for (auto* parent = source.mParent; parent; parent = parent->mParent) {
        meshToModel = AssimpGLMHelpers::ConvertMatrixToGLMFormat(parent->mTransformation) * meshToModel;
    }
    return meshToModel;
}

MeshBindTransforms makeMeshBindTransforms(const aiMesh& source, const glm::mat4& meshToModelBind) {
    const glm::dmat4 meshToModel(meshToModelBind);
    if (!finiteMatrix(meshToModel)) {
        throw std::runtime_error("Non-finite mesh bind transform: " + std::string(source.mName.C_Str()));
    }
    const double determinant = glm::determinant(meshToModel);
    const bool invertible = determinant != 0.0 && std::isfinite(determinant);
    const glm::dmat4 modelToMesh = invertible ? glm::inverse(meshToModel) : glm::dmat4(1.0);
    if (source.mNumBones && (!invertible || !finiteMatrix(modelToMesh))) {
        throw std::runtime_error("Unsupported non-invertible skinned mesh bind transform: " + std::string(source.mName.C_Str()));
    }
    const glm::dmat3 linearTransform(meshToModel);
    // Cofactors still provide a finite normal direction for a collapsed static
    // mesh. Invertible meshes retain inverse-transpose, including reflected ones.
    const glm::dmat3 normalTransform = invertible && finiteMatrix(modelToMesh)
        ? glm::transpose(glm::dmat3(modelToMesh))
        : glm::dmat3(glm::cross(linearTransform[1], linearTransform[2]),
            glm::cross(linearTransform[2], linearTransform[0]), glm::cross(linearTransform[0], linearTransform[1]));
    return {meshToModelBind, modelToMesh, linearTransform, normalTransform};
}

glm::vec3 normalizedDirection(const glm::dvec3& direction) {
    const double length = glm::length(direction);
    return length > 0.0 && std::isfinite(length) ? glm::vec3(direction / length) : glm::vec3(0.0f);
}

glm::vec3 toGlm(const aiVector3D& value) {
    return glm::vec3(value.x, value.y, value.z);
}

glm::vec3 importDirection(const aiVector3D* values, unsigned int index, const glm::dmat3& toModel) {
    if (!values) return glm::vec3(0.0f);
    return normalizedDirection(toModel * glm::dvec3(toGlm(values[index])));
}

std::vector<Vertex> importVertices(const aiMesh& source, const MeshBindTransforms& bind) {
    std::vector<Vertex> vertices;
    for (unsigned int index = 0; index < source.mNumVertices; ++index) {
        Vertex vertex;
        vertex.position = bind.meshToModel * glm::vec4(toGlm(source.mVertices[index]), 1.0f);
        vertex.normal = importDirection(source.mNormals, index, bind.normalToModel);
        vertex.tangent = importDirection(source.mTangents, index, bind.directionToModel);
        vertex.bitangent = importDirection(source.mBitangents, index, bind.directionToModel);
        vertex.texCoords = source.mTextureCoords[0]
            ? glm::vec2(source.mTextureCoords[0][index].x, source.mTextureCoords[0][index].y)
            : glm::vec2(0.0f);
        for (int slot = 0; slot < TOGR_MAX_BONE_INFLUENCE; ++slot) {
            vertex.boneIDs[slot] = -1;
            vertex.weights[slot] = -1.0f;
        }
        vertices.push_back(vertex);
    }
    return vertices;
}

void importBoneWeights(const aiBone& source, int boneId, std::vector<Vertex>& vertices) {
    for (unsigned int index = 0; index < source.mNumWeights; ++index) {
        const unsigned int vertexId = source.mWeights[index].mVertexId;
        if (vertexId >= vertices.size()) {
            TOGR_LOG_WARN("Skipping bone weight with invalid vertex index: " << vertexId);
            continue;
        }
        for (int slot = 0; slot < TOGR_MAX_BONE_INFLUENCE; ++slot) {
            if (vertices[vertexId].boneIDs[slot] < 0) {
                vertices[vertexId].boneIDs[slot] = boneId;
                vertices[vertexId].weights[slot] = source.mWeights[index].mWeight;
                break;
            }
        }
    }
}

std::vector<GLuint> importIndices(const aiMesh& source) {
    std::vector<GLuint> indices;
    for (unsigned int face = 0; face < source.mNumFaces; ++face) {
        const aiFace& sourceFace = source.mFaces[face];
        for (unsigned int index = 0; index < sourceFace.mNumIndices; ++index) {
            indices.push_back(sourceFace.mIndices[index]);
        }
    }
    return indices;
}

std::string resolveTexturePath(const std::filesystem::path& modelDirectory, const aiString& texturePath) {
    if (texturePath.length == 0) return {};
    return (modelDirectory / std::filesystem::u8path(texturePath.C_Str())).u8string();
}

Material importMaterialPaths(const aiMaterial& source, const std::filesystem::path& modelDirectory) {
    aiString albedoPath;
    aiString ambientPath;
    aiString specularPath;
    aiString normalPath;
    aiString shininessPath;
    source.GetTexture(aiTextureType_DIFFUSE, 0, &albedoPath);
    source.GetTexture(aiTextureType_AMBIENT, 0, &ambientPath);
    source.GetTexture(aiTextureType_SPECULAR, 0, &specularPath);
    source.GetTexture(aiTextureType_HEIGHT, 0, &normalPath);
    source.GetTexture(aiTextureType_SHININESS, 0, &shininessPath);
    return Material(resolveTexturePath(modelDirectory, albedoPath),
        resolveTexturePath(modelDirectory, ambientPath), resolveTexturePath(modelDirectory, specularPath),
        resolveTexturePath(modelDirectory, normalPath), resolveTexturePath(modelDirectory, shininessPath));
}

bool finiteMatrix(const glm::dmat4& matrix) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) return false;
        }
    }
    return true;
}

bool compatibleOffsets(const glm::mat4& first, const glm::mat4& second) {
    constexpr double absoluteTolerance = 1e-5;
    constexpr double relativeTolerance = 1e-4;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            const double a = first[column][row];
            const double b = second[column][row];
            if (std::abs(a - b) > absoluteTolerance + relativeTolerance * std::max(std::abs(a), std::abs(b))) {
                return false;
            }
        }
    }
    return true;
}
}

} // namespace togr
