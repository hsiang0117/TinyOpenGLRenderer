#ifndef MESH_GENERATOR_HPP
#define MESH_GENERATOR_HPP
#pragma once

#include "mesh.hpp"

class MeshGenerator {
public:
	static MeshPtr generateCube();
	static MeshPtr generateSphere(int sectorCount = 32, int stackCount = 32);
	static MeshPtr generatePlane();
private:
	static void computeTangents(std::vector<Vertex>& vertices, std::vector<GLuint>& indices);
	static void setDefaultBoneData(std::vector<Vertex>& vertices); //先这样搞吧，后续再想办法优化
};

MeshPtr MeshGenerator::generateCube() {
	static Mesh cube;
	if (!cube.isReady()) {
		cube.vertices = {
			// Back face
			{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f)}, // Bottom-left
			{glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f)}, // top-right
			{glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f)}, // bottom-right
			{glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f)},  // top-right
			{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f)},  // bottom-left
			{glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f)},// top-left
			// Front face
			{glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)}, // bottom-left
			{glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)},  // bottom-right
			{glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)},  // top-right
			{glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)}, // top-right
			{glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)},  // top-left
			{glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)},  // bottom-left
			// Left face
			{glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)}, // top-right
			{glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f)}, // top-left
			{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)},  // bottom-left
			{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)}, // bottom-left
			{glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)},  // bottom-right
			{glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)}, // top-right
			// Right face
			{glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)}, // top-left
			{glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)}, // bottom-right
			{glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f)}, // top-right
			{glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)},  // bottom-right
			{glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)},  // top-left
			{glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // bottom-left
			// Bottom face
			{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f)}, // top-right
			{glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f)}, // top-left
			{glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)},// bottom-left
			{glm::vec3(0.5f, -0.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)}, // bottom-left
			{glm::vec3(-0.5f, -0.5f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)}, // bottom-right
			{glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f)}, // top-right
			// Top face
			{glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},// top-left
			{glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)}, // bottom-right
			{glm::vec3(0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)}, // top-right
			{glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)}, // bottom-right
			{glm::vec3(-0.5f, 0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)},// top-left
			{glm::vec3(-0.5f, 0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)} // bottom-left
		};
		cube.indices = {
			// Back face
			 0,  1,  2,
			 3,  4,  5,
			 // Front face
			  6,  7,  8,
			  9, 10, 11,
			  // Left face
			  12, 13, 14,
			  15, 16, 17,
			  // Right face
			  18, 19, 20,
			  21, 22, 23,
			  // Bottom face
			  24, 25, 26,
			  27, 28, 29,
			  // Top face
			  30, 31, 32,
			  33, 34, 35
		};
		computeTangents(cube.vertices, cube.indices);
		setDefaultBoneData(cube.vertices);
		cube.initGLResources();
	}
	return std::make_shared<Mesh>(cube);
}

MeshPtr MeshGenerator::generateSphere(int sectorCount, int stackCount)
{
	static Mesh sphere;
	if (!sphere.isReady()) {
		std::vector<Vertex> vertices;
		std::vector<GLuint> indices;
		const float PI = 3.14159265359f;

		for (int i = 0; i <= stackCount; ++i) {
			float stackAngle = PI / 2 - i * PI / stackCount; // from pi/2 to -pi/2
			float xy = cosf(stackAngle);
			float z = sinf(stackAngle);
			for (int j = 0; j <= sectorCount; ++j) {
				float sectorAngle = j * 2 * PI / sectorCount; // 0 to 2pi
				float x = xy * cosf(sectorAngle);
				float y = xy * sinf(sectorAngle);
				Vertex v;
				v.position = glm::vec3(x, y, z);
				v.normal = glm::normalize(v.position);
				v.texCoords = glm::vec2((float)j / sectorCount, (float)i / stackCount);
				vertices.push_back(v);
			}
		}

		for (int i = 0; i < stackCount; ++i) {
			int k1 = i * (sectorCount + 1);
			int k2 = k1 + sectorCount + 1;
			for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
				if (i != 0) {
					indices.push_back(k1);
					indices.push_back(k2);
					indices.push_back(k1 + 1);
				}
				if (i != (stackCount - 1)) {
					indices.push_back(k1 + 1);
					indices.push_back(k2);
					indices.push_back(k2 + 1);
				}
			}
		}

		sphere.vertices = vertices;
		sphere.indices = indices;
		computeTangents(sphere.vertices, sphere.indices);
		setDefaultBoneData(sphere.vertices);
		sphere.initGLResources();
	}
	return std::make_shared<Mesh>(sphere);
}

MeshPtr MeshGenerator::generatePlane()
{
	static Mesh plane;
	if (!plane.isReady()) {
		plane.vertices = {
			{ glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f) },
			{ glm::vec3(0.5f, 0.0f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f) },
			{ glm::vec3(0.5f, 0.0f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f) },
			{ glm::vec3(-0.5f, 0.0f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f) }
		};
		plane.indices = {
			// First triangle
			0, 1, 2,
			// Second triangle
			0, 2, 3
		};
		computeTangents(plane.vertices, plane.indices);
		setDefaultBoneData(plane.vertices);
		plane.initGLResources();
	}
	return std::make_shared<Mesh>(plane);
}

void MeshGenerator::computeTangents(std::vector<Vertex>& vertices, std::vector<GLuint>& indices)
{
	for (auto& v : vertices) {
		v.tangent = glm::vec3(0.0f);
		v.bitangent = glm::vec3(0.0f);
	}
	// accumulate
	for (size_t i = 0; i + 2 < indices.size(); i += 3) {
		Vertex& v0 = vertices[indices[i + 0]];
		Vertex& v1 = vertices[indices[i + 1]];
		Vertex& v2 = vertices[indices[i + 2]];
		glm::vec3 edge1 = v1.position - v0.position;
		glm::vec3 edge2 = v2.position - v0.position;
		glm::vec2 deltaUV1 = v1.texCoords - v0.texCoords;
		glm::vec2 deltaUV2 = v2.texCoords - v0.texCoords;
		float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
		glm::vec3 tangent = f * (edge1 * deltaUV2.y - edge2 * deltaUV1.y);
		glm::vec3 bitangent = f * (edge2 * deltaUV1.x - edge1 * deltaUV2.x);
		v0.tangent += tangent;
		v1.tangent += tangent;
		v2.tangent += tangent;
		v0.bitangent += bitangent;
		v1.bitangent += bitangent;
		v2.bitangent += bitangent;
	}
	// normalize
	for (auto& v : vertices) {
		v.tangent = glm::normalize(v.tangent);
		v.bitangent = glm::normalize(v.bitangent);
	}
}

void MeshGenerator::setDefaultBoneData(std::vector<Vertex>& vertices)
{
	for (auto& v : vertices) {
		for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
			v.boneIDs[i] = -1;
			v.weights[i] = -1.0f;
		}
	}
}

#endif