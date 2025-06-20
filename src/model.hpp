#ifndef MODEL_HPP
#define MODEL_HPP
#pragma once

#include "material.hpp"
#include "mesh.hpp"
#include <unordered_map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <future>

class Model
{
public:
	Model() = default;
	// 异步加载资源
	static std::future<std::shared_ptr<Model>> LoadAsync(const char* path);
	// 主线程调用，初始化opengl资源
	bool initGLResources();

	std::string getPath() { return path; }
	std::string getName() { return name; }
	void draw(ShaderPtr shader);
	bool isReady() const { return loaded && glInitialized; }
	void buildAABB(glm::vec3& min, glm::vec3& max);
private:
	bool loaded = false, glInitialized = false;

	std::unordered_map<unsigned int, Material> materials;
	std::vector<MeshPtr> meshes;

	std::string path;
	std::string directory;
	std::string name;

	void loadModel(std::string path);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
};

std::future<std::shared_ptr<Model>> Model::LoadAsync(const char* path) {
	std::string modelPathStr(path);
	return std::async(std::launch::async, [modelPathStr]() {
		auto model = std::make_shared<Model>();
		model->loadModel(modelPathStr.c_str());
		return model;
		});
}

bool Model::initGLResources()
{
	if (!loaded || glInitialized) return false;

	for (auto& mesh : meshes) {
		if (!mesh->initGLResources()) {
			std::cerr << "Failed to initialize mesh GL resources\n";
			return false;
		}
	}
	for (auto it = materials.begin(); it != materials.end(); ++it) {
		it->second.initGLResources();
	}
	glInitialized = true;
	return true;
}

void Model::draw(ShaderPtr shader)
{
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		auto material = materials.find(meshes[i]->getMaterialIndex());
		if (material != materials.end()) {
			material->second.bind(shader);
		}
		meshes[i]->draw();
	}
}

void Model::buildAABB(glm::vec3& min, glm::vec3& max)
{
	// 初始化min和max为极值
	min = glm::vec3(std::numeric_limits<float>::max());
	max = glm::vec3(std::numeric_limits<float>::lowest());

	for (const auto& meshPtr : meshes) {
		if (!meshPtr) continue;
		for (const auto& vertex : meshPtr->vertices) {
			min.x = std::min(min.x, vertex.position.x);
			min.y = std::min(min.y, vertex.position.y);
			min.z = std::min(min.z, vertex.position.z);

			max.x = std::max(max.x, vertex.position.x);
			max.y = std::max(max.y, vertex.position.y);
			max.z = std::max(max.z, vertex.position.z);
		}
	}
}

void Model::loadModel(std::string path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return;
	}

	this->path = path;
	directory = path.substr(0, path.find_last_of('\\')) + "\\";
	name = path.substr(path.find_last_of('\\') + 1, path.find_first_of('.') - path.find_last_of('\\') - 1);
	processNode(scene->mRootNode, scene);
	importer.FreeScene();
	loaded = true;
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		MeshPtr meshPtr = std::make_shared<Mesh>(processMesh(mesh, scene));
		meshes.push_back(meshPtr);
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	Mesh result;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		glm::vec3 vector;
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.position = vector;
		if (mesh->mNormals)
		{
			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.normal = vector;
		}
		else
		{
			vertex.normal = glm::vec3(0.0f);
		}
		if (mesh->mTextureCoords[0])
		{
			glm::vec2 vec;
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.texCoords = vec;
		}
		else
		{
			vertex.texCoords = glm::vec2(0.0f);
		}
		if (mesh->mTangents)
		{
			vector.x = mesh->mTangents[i].x;
			vector.y = mesh->mTangents[i].y;
			vector.z = mesh->mTangents[i].z;
			vertex.tangent = vector;
		}
		else
		{
			vertex.tangent = glm::vec3(0.0f);
		}
		if (mesh->mBitangents)
		{
			vector.x = mesh->mBitangents[i].x;
			vector.y = mesh->mBitangents[i].y;
			vector.z = mesh->mBitangents[i].z;
			vertex.bitangent = vector;
		}
		else
		{
			vertex.bitangent = glm::vec3(0.0f);
		}
		result.vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			result.indices.push_back(face.mIndices[j]);
		}
	}

	if (mesh->mMaterialIndex >= 0)
	{
		result.setMaterialIndex(mesh->mMaterialIndex);
		Material mat;
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		if (materials.find(mesh->mMaterialIndex) == materials.end()) {
			aiString albedoPath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoPath);
			aiString ambientPath;
			material->GetTexture(aiTextureType_AMBIENT, 0, &ambientPath);
			aiString specularPath;
			material->GetTexture(aiTextureType_SPECULAR, 0, &specularPath);
			aiString normalPath;
			material->GetTexture(aiTextureType_HEIGHT, 0, &normalPath);
			aiString shininessPath;
			material->GetTexture(aiTextureType_SHININESS, 0, &shininessPath);
			mat = Material(mesh->mMaterialIndex, directory + albedoPath.C_Str(), directory + ambientPath.C_Str(),
				directory + specularPath.C_Str(), directory + normalPath.C_Str(), directory + shininessPath.C_Str());
			materials.insert({ mesh->mMaterialIndex, mat });
		}
	}

	return result;
}

using ModelPtr = std::shared_ptr<Model>;

#endif // !MODEL_HPP