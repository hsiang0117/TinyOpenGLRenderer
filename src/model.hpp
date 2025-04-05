#ifndef MODEL_HPP
#define MODEL_HPP
#pragma once

#include "mesh.hpp"
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

	void draw(Shader& shader);
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
		Material material;
	};

	std::vector<MeshData> meshdatas;
	std::vector<Mesh> meshes;

	std::string directory;

	void loadModel(std::string path);
	void processNode(aiNode* node, const aiScene* scene);
	MeshData processMesh(aiMesh* mesh, const aiScene* scene);
};

std::future<Model> Model::LoadAsync(const char* path) {
	return std::async(std::launch::async, [path]() {
		Model model;
		model.loadModel(path);
		return model;
	});
}

bool Model::initGLResources()
{
	if (!loaded || glInitialized) return false;

	for (auto& data : meshdatas) {
		meshes.emplace_back(data.vertices, data.indices, data.material);
		if (!meshes.back().initGLResources()) {
			std::cerr << "Failed to initialize mesh GL resources\n";
			return false;
		}
	}

	meshdatas.clear(); // 释放临时数据
	glInitialized = true;
	return true;
}

void Model::draw(Shader& shader)
{
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, translate);
	model = glm::rotate(model, radians, axis);
	model = glm::scale(model, scale);
	shader.setMat4("model", model);
	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		meshes[i].draw(shader);
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
	directory = path.substr(0, path.find_last_of('/')) + "/";
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

	Material mat;
	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		aiString name;
		material->Get(AI_MATKEY_NAME, name);
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
	}
	data.material = mat;
	return data;
}

#endif // !MODEL_HPP