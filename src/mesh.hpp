#ifndef MESH_HPP
#define MESH_HPP
#pragma once

#include "shader.hpp"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
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

using MeshPtr = std::shared_ptr<Mesh>;
#endif