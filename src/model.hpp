#ifndef MODEL_HPP
#define MODEL_HPP
#pragma once

#include "material.hpp"
#include "mesh.hpp"
#include "utils.hpp"
#include "assimpNode.hpp"
#include "animation.hpp"
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
	void drawBones();
	bool isReady() const { return loaded && glInitialized; }
	void buildAABB(glm::vec3& min, glm::vec3& max);
	Node* findNode(std::string name);
	std::vector<Node>& getAllNodes() { return nodes; }
	std::vector<Animation>& getAnimations() { return animations; }
	Texture2D& getBoneMatrixTexture() { return boneMatrixTexture; }
private:
	bool loaded = false, glInitialized = false;

	std::unordered_map<unsigned int, Material> materials;
	std::vector<MeshPtr> meshes;
	std::vector<Node> nodes;
	std::vector<Animation> animations;

	std::string path;
	std::string directory;
	std::string name;

	void loadModel(std::string path);
	void processNode(aiNode* node, const aiScene* scene, int parentIndex);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 nodeTransform);

	Texture2D boneMatrixTexture;

	GLuint VAO, VBO, lineVAO, lineVBO; //渲染骨骼节点用
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
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenVertexArrays(1, &lineVAO);
	glGenBuffers(1, &lineVBO);

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

void Model::drawBones()
{
	std::vector<glm::vec3> bonePositions;
	for (const auto& node : nodes) {
		if (node.isBoneNode) {
			bonePositions.push_back(node.position);
		}
	}
	if (!bonePositions.empty()) {
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, bonePositions.size() * sizeof(glm::vec3), &bonePositions[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(bonePositions.size()));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	std::vector<glm::vec3> lineVertices;
	for(int i = 0;i<nodes.size(); i++) {
		if (nodes[i].isBoneNode && nodes[nodes[i].parentIndex].isBoneNode && nodes[i].parentIndex != -1) {
			lineVertices.push_back(nodes[i].position);
			lineVertices.push_back(nodes[nodes[i].parentIndex].position);
		}
	}

	if (!lineVertices.empty()) {
		glBindVertexArray(lineVAO);
		glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
		glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(glm::vec3), lineVertices.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lineVertices.size()));
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
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
	processNode(scene->mRootNode, scene, -1);
	for(auto i = 0; i<scene->mNumAnimations; i++) {
		animations.emplace_back(scene->mAnimations[i], nodes);
	}
	importer.FreeScene();
	loaded = true;
}

void Model::processNode(aiNode* node, const aiScene* scene, int parentIndex)
{	
	int currentIndex;
	if (findNode(node->mName.C_Str()) == nullptr) {
		Node n;
		n.name = node->mName.C_Str();
		n.id = static_cast<int>(nodes.size());
		n.parentIndex = parentIndex;
		if(n.parentIndex != -1)
			nodes[n.parentIndex].childrenIndices.push_back(n.id);
		n.transform = AssimpGLMHelpers::ConvertMatrixToGLMFormat(node->mTransformation);
		n.position = n.transform * glm::vec4(0, 0, 0, 1);
		while(parentIndex != -1) {
			n.position = nodes[parentIndex].transform * glm::vec4(n.position,1.0f);
			parentIndex = nodes[parentIndex].parentIndex;
		}
		n.offsetMatrix = glm::mat4(1.0f);
		nodes.push_back(n);
		currentIndex = n.id;
	}
	else {
		auto it = std::find_if(nodes.begin(), nodes.end(), [&node](const Node& n) {
			return n.name == node->mName.C_Str();
			});
		if (it != nodes.end()) {
			it->parentIndex = parentIndex;
			if (it->parentIndex != -1)
				nodes[it->parentIndex].childrenIndices.push_back(it->id);
			it->transform = AssimpGLMHelpers::ConvertMatrixToGLMFormat(node->mTransformation);
			it->position = it->transform * glm::vec4(0, 0, 0, 1);
			while (parentIndex != -1) {
				it->position = nodes[parentIndex].transform * glm::vec4(it->position, 1.0f);
				parentIndex = nodes[parentIndex].parentIndex;
			} 
			currentIndex = it->id;
		}
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene, currentIndex);
	}
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		auto nodeTransform = AssimpGLMHelpers::ConvertMatrixToGLMFormat(node->mTransformation);
		auto parent = node->mParent;
		while (parent) {
			nodeTransform = AssimpGLMHelpers::ConvertMatrixToGLMFormat(parent->mTransformation) * nodeTransform;
			parent = parent->mParent;
		}
		MeshPtr meshPtr = std::make_shared<Mesh>(processMesh(mesh, scene, nodeTransform));
		meshes.push_back(meshPtr);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 nodeTransform)
{
	Mesh result;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		glm::vec3 vector;
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vector = nodeTransform * glm::vec4(vector, 1.0f);
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
		for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
			vertex.boneIDs[i] = -1;
			vertex.weights[i] = -1.0f;
		}
		result.vertices.push_back(vertex);
	}

	for (int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
		int boneID = -1;
		std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
		if (findNode(boneName) == nullptr) {
			Node node;
			node.name = boneName;
			node.id = static_cast<int>(nodes.size());
			node.parentIndex = -1;
			node.transform = glm::mat4(1.0f);
			node.offsetMatrix = AssimpGLMHelpers::ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
			node.position = glm::vec3(0.0f);
			node.isBoneNode = true;
			nodes.push_back(node);
			boneID = node.id;
		}
		else {
			auto it = std::find_if(nodes.begin(), nodes.end(), [&boneName](const Node& node) {
				return node.name == boneName;
				});
			if (it != nodes.end()) {
				it->offsetMatrix = AssimpGLMHelpers::ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
				it->isBoneNode = true;
				boneID = it->id;
			}
		}
		assert(boneID != -1);
		auto weights = mesh->mBones[boneIndex]->mWeights;
		int numWeights = mesh->mBones[boneIndex]->mNumWeights;
		for (int weightIndex = 0; weightIndex < numWeights; weightIndex++) {
			int vertexID = weights[weightIndex].mVertexId;
			float weight = weights[weightIndex].mWeight;
			assert(vertexID < result.vertices.size());
			for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
				if (result.vertices[vertexID].boneIDs[i] < 0) {
					result.vertices[vertexID].boneIDs[i] = boneID;
					result.vertices[vertexID].weights[i] = weight;
					break;
				}
			}
		}
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

Node* Model::findNode(std::string name)
{
	for (auto& node : nodes) {
		if (node.name == name) {
			return &node;
		}
	}
	return nullptr;
}

using ModelPtr = std::shared_ptr<Model>;

#endif // !MODEL_HPP