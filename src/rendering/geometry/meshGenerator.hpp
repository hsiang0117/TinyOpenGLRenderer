#ifndef TOGR_MESH_GENERATOR_HPP
#define TOGR_MESH_GENERATOR_HPP
#pragma once

#include "mesh.hpp"

namespace togr {

class MeshGenerator {
public:
	static MeshPtr generateCube();
	static MeshPtr generateSphere(int sectorCount = 32, int stackCount = 32);
	static MeshPtr generatePlane();
private:
	static void computeTangents(std::vector<Vertex>& vertices, std::vector<GLuint>& indices);
	static void setDefaultBoneData(std::vector<Vertex>& vertices);
};

} // namespace togr

#endif
