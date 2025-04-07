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
	// 异步加载资源
	static std::future<Model> LoadAsync(const char* path);
	// 主线程调用，初始化opengl资源
	bool initGLResources();
	
	std::string getPath() { return path; }
	std::string getName() { return name; }
	void draw(ShaderPtr shader);
	bool isReady() const { return loaded && glInitialized; }
private:
	Model() = default;
	bool loaded = false, glInitialized = false;

	glm::vec3 scale = glm::vec3(1.0f),
		translate = glm::vec3(1.0f),
		axis = glm::vec3(1.0f);
	float radians = 0;

	struct MeshData {
		std::vector<Vertex> vertices;
		std::vector<GLuint> indices;
	};

	std::unordered_map<std::string, Material> materials;
	std::vector<MeshData> meshdatas;
	std::vector<Mesh> meshes;

	std::string path;
	std::string directory;
	std::string name;

	void loadModel(std::string path);
	void processNode(aiNode* node, const aiScene* scene);
	MeshData processMesh(aiMesh* mesh, const aiScene* scene);
};

std::future<Model> Model::LoadAsync(const char* path) {
	std::string modelPathStr(path);
	return std::async(std::launch::async, [modelPathStr]() {
		Model model;
		model.loadModel(modelPathStr.c_str());
		return model;
	});
}

bool Model::initGLResources()
{
	if (!loaded || glInitialized) return false;

	for (auto& data : meshdatas) {
		meshes.emplace_back(data.vertices, data.indices);
		if (!meshes.back().initGLResources()) {
			std::cerr << "Failed to initialize mesh GL resources\n";
			return false;
		}
	}
	meshdatas.clear(); // 释放临时数据
	for (auto it = materials.begin(); it != materials.end(); ++it) {
		it->second.initGLResources();
	}
	glInitialized = true;
	return true;
}

void Model::draw(ShaderPtr shader)
{
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, translate);
	model = glm::rotate(model, radians, axis);
	model = glm::scale(model, scale);
	shader.get()->setMat4("model", model);
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		meshes[i].draw();
	}
}

void Model::loadModel(std::string path)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return;
	}
	
	this->path = path;
	directory = path.substr(0, path.find_last_of('/')) + "/";
	name = path.substr(path.find_last_of('/') + 1, path.find_first_of('.') - path.find_last_of('/') - 1);
	processNode(scene->mRootNode, scene);
	importer.FreeScene();
	loaded = true;
}

void Model::processNode(aiNode* node, const aiScene* scene)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshdatas.push_back(processMesh(mesh, scene));
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
}

Model::MeshData Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	MeshData data;

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
		data.vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			data.indices.push_back(face.mIndices[j]);
		}
	}

	if (mesh->mMaterialIndex >= 0)
	{
		Material mat;
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		aiString name;
		material->Get(AI_MATKEY_NAME, name);
		if (materials.find(name.C_Str()) == materials.end()) {
			aiString albedoPath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &albedoPath);
			aiString ambientPath;
			material->GetTexture(aiTextureType_AMBIENT, 0, &ambientPath);
			aiString specularPath;
			material->GetTexture(aiTextureType_SPECULAR, 0, &specularPath);
			aiString normalPath;
			material->GetTexture(aiTextureType_NORMALS, 0, &normalPath);
			aiString shininessPath;
			material->GetTexture(aiTextureType_SHININESS, 0, &shininessPath);
			mat = Material(name.C_Str(), directory + albedoPath.C_Str(), directory + ambientPath.C_Str(),
				directory + specularPath.C_Str(), directory + normalPath.C_Str(), directory + shininessPath.C_Str());
			materials.insert({ name.C_Str(), mat });
		}
	}

	return data;
}

using ModelPtr = std::shared_ptr<Model>;

#endif // !MODEL_HPP