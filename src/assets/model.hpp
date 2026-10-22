#ifndef MODEL_HPP
#define MODEL_HPP
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

class Shader;

struct aiNode;
struct aiScene;
struct aiMesh;

class Model
{
public:
	Model() = default;
	static std::future<std::shared_ptr<Model>> LoadAsync(const char* path);
	bool initGLResources();

	std::string getPath() { return path.u8string(); }
	std::string getName() { return name; }
	void draw(std::shared_ptr<Shader> shader);
	bool isReady() const { return loaded && glInitialized; }
	void buildAABB(glm::vec3& min, glm::vec3& max);
	Node* findNode(std::string name);
	std::vector<Node>& getNodes() { return nodes; }
	std::vector<Animation>& getAnimations() { return animations; }
private:
	bool loaded = false, glInitialized = false;

	std::unordered_map<unsigned int, Material> materials;
	std::vector<MeshPtr> meshes;
	std::vector<Node> nodes;
	std::vector<Animation> animations;

	std::filesystem::path path;
	std::filesystem::path directory;
	std::string name;

	void loadModel(const std::filesystem::path& modelPath);
	void processNode(aiNode* node, const aiScene* scene, int parentIndex);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 meshToModelBind);
};

using ModelPtr = std::shared_ptr<Model>;

#endif // !MODEL_HPP
