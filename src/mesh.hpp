#ifndef MESH_HPP
#define MESH_HPP
#pragma once

#include "shader.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#define MAX_BONE_INFLUENCE 4

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
	glm::vec3 tangent;
	glm::vec3 bitangent;
	int boneIDs[MAX_BONE_INFLUENCE];
    float weights[MAX_BONE_INFLUENCE];
};

class Mesh
{
public:
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    Mesh() = default;
    Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices);
    bool initGLResources();
	bool isReady() const { return glInitialized; }
    void draw();
    void setMaterialIndex(unsigned int index) { materialIndex = index; }
    unsigned int getMaterialIndex() { return materialIndex; }
    void buildAABB(glm::vec3& min, glm::vec3& max);
private:
    GLuint VAO, VBO, EBO;
    unsigned int materialIndex;
    bool glInitialized = false;
    void setupMesh();
};

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices)
{
    this->vertices = vertices;
    this->indices = indices;
}

bool Mesh::initGLResources()
{
    if (glInitialized || vertices.empty() || indices.empty())
        return false;
    setupMesh();
    glInitialized = true;
    return true;
}

void Mesh::setupMesh()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));
	glEnableVertexAttribArray(4);
	glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, boneIDs));
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, weights));
	glEnableVertexAttribArray(6);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Mesh::draw()
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Mesh::buildAABB(glm::vec3& min, glm::vec3& max) {
    min = glm::vec3(std::numeric_limits<float>::max());
    max = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& vertex : vertices) {
        min.x = std::min(min.x, vertex.position.x);
        min.y = std::min(min.y, vertex.position.y);
        min.z = std::min(min.z, vertex.position.z);
        max.x = std::max(max.x, vertex.position.x);
        max.y = std::max(max.y, vertex.position.y);
        max.z = std::max(max.z, vertex.position.z);
    }
}

using MeshPtr = std::shared_ptr<Mesh>;
#endif