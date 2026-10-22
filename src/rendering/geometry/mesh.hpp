#ifndef TOGR_MESH_HPP
#define TOGR_MESH_HPP
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

#define TOGR_MAX_BONES 500
#define TOGR_MAX_BONE_INFLUENCE 4

namespace togr {

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
	glm::vec3 tangent;
	glm::vec3 bitangent;
	int boneIDs[TOGR_MAX_BONE_INFLUENCE];
    float weights[TOGR_MAX_BONE_INFLUENCE];
};

class Mesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    bool initGLResources();
    void draw();
    void setMaterialIndex(unsigned int index) { materialIndex = index; }
    unsigned int getMaterialIndex() { return materialIndex; }
    void buildAABB(glm::vec3& min, glm::vec3& max);
private:
    GLuint VAO = 0, VBO = 0, EBO = 0;
    unsigned int materialIndex = 0;
    bool glInitialized = false;
    void setupMesh();
};

using MeshPtr = std::shared_ptr<Mesh>;

} // namespace togr

#endif
