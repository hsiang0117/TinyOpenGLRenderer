#include "model.hpp"
#include "rendering/opengl/shader.hpp"
#include "assimpGlmHelpers.hpp"
#include "assimpFileSystem.hpp"
#include "core/logger.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
std::string resolveTexturePath(const std::filesystem::path& modelDirectory, const aiString& texturePath)
{
	if (texturePath.length == 0) return {};
	return (modelDirectory / std::filesystem::u8path(texturePath.C_Str())).u8string();
}

bool finiteMatrix(const glm::dmat4& matrix)
{
	for (int column = 0; column < 4; ++column)
		for (int row = 0; row < 4; ++row)
			if (!std::isfinite(matrix[column][row])) return false;
	return true;
}

bool compatibleOffsets(const glm::mat4& first, const glm::mat4& second)
{
	for (int column = 0; column < 4; ++column) {
		for (int row = 0; row < 4; ++row) {
			const double a = first[column][row], b = second[column][row];
			if (std::abs(a - b) > 1e-5 + 1e-4 * std::max(std::abs(a), std::abs(b))) return false;
		}
	}
	return true;
}

glm::vec3 normalizedDirection(const glm::dvec3& direction)
{
	const double length = glm::length(direction);
	return length > 0.0 && std::isfinite(length) ? glm::vec3(direction / length) : glm::vec3(0.0f);
}
}

std::future<std::shared_ptr<Model>> Model::LoadAsync(const char* path) {
	const auto modelPath = std::filesystem::u8path(path ? path : "");
	return std::async(std::launch::async, [modelPath]() {
		auto model = std::make_shared<Model>();
		model->loadModel(modelPath);
		return model;
		});
}

bool Model::initGLResources()
{
	if (!loaded || glInitialized) return false;

	for (auto& mesh : meshes) {
		if (!mesh->initGLResources()) {
			LOG_ERROR("Failed to initialize mesh GL resources");
			return false;
		}
	}
	for (auto it = materials.begin(); it != materials.end(); ++it) {
		if (!it->second.initGLResources())
			LOG_WARN("Model uses defaults for failed material textures: " << path.u8string() << " (material " << it->first << ")");
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

void Model::loadModel(const std::filesystem::path& modelPath)
{
	Assimp::Importer importer;
	importer.SetIOHandler(new AssimpFileSystem());
	const aiScene* scene = importer.ReadFile(modelPath.u8string(), aiProcess_Triangulate | aiProcess_CalcTangentSpace);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		LOG_ERROR("ERROR::ASSIMP::" << importer.GetErrorString());
		return;
	}
	path = modelPath;
	directory = modelPath.parent_path();
	name = modelPath.stem().u8string();
	try {
		processNode(scene->mRootNode, scene, -1);
		for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
			animations.emplace_back(scene->mAnimations[i], nodes);
		}
	} catch (const std::runtime_error& error) {
		LOG_ERROR("Cannot import model " << modelPath.u8string() << ": " << error.what());
		meshes.clear();
		materials.clear();
		nodes.clear();
		animations.clear();
		return;
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

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 meshToModelBind)
{
	Mesh result;
	const glm::dmat4 meshToModel(meshToModelBind);
	if (!finiteMatrix(meshToModel))
		throw std::runtime_error("Non-finite mesh bind transform: " + std::string(mesh->mName.C_Str()));
	const double determinant = glm::determinant(meshToModel);
	const bool invertible = determinant != 0.0 && std::isfinite(determinant);
	const glm::dmat4 modelToMesh = invertible ? glm::inverse(meshToModel) : glm::dmat4(1.0);
	if (mesh->mNumBones && (!invertible || !finiteMatrix(modelToMesh)))
		throw std::runtime_error("Unsupported non-invertible skinned mesh bind transform: " + std::string(mesh->mName.C_Str()));
	const glm::dmat3 linearTransform(meshToModel);
	// Cofactors still provide a finite normal direction for a collapsed static mesh.
	const glm::dmat3 normalTransform = invertible && finiteMatrix(modelToMesh)
		? glm::transpose(glm::dmat3(modelToMesh))
		: glm::dmat3(glm::cross(linearTransform[1], linearTransform[2]),
			glm::cross(linearTransform[2], linearTransform[0]), glm::cross(linearTransform[0], linearTransform[1]));

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		glm::vec3 vector;
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vector = meshToModelBind * glm::vec4(vector, 1.0f);
		vertex.position = vector;
		if (mesh->mNormals)
		{
			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.normal = normalizedDirection(normalTransform * glm::dvec3(vector));
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
			vertex.tangent = normalizedDirection(linearTransform * glm::dvec3(vector));
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
			vertex.bitangent = normalizedDirection(linearTransform * glm::dvec3(vector));
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
		const glm::dmat4 meshOffset(AssimpGLMHelpers::ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix));
		// Vertices are already in model space. Convert the Assimp mesh-local
		// offset so G(t) * offset * (meshToModelBind * p) equals G(t) * O * p.
		const glm::mat4 modelOffset(meshOffset * modelToMesh);
		if (!finiteMatrix(meshOffset) || !finiteMatrix(glm::dmat4(modelOffset)))
			throw std::runtime_error("Non-finite skin binding for bone: " + boneName);
		if (findNode(boneName) == nullptr) {
			Node node;
			node.name = boneName;
			node.id = static_cast<int>(nodes.size());
			node.parentIndex = -1;
			node.transform = glm::mat4(1.0f);
			node.offsetMatrix = modelOffset;
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
				if (it->isBoneNode && !compatibleOffsets(it->offsetMatrix, modelOffset))
					throw std::runtime_error("Unsupported incompatible mesh bindings for shared bone: " + boneName);
				if (!it->isBoneNode) it->offsetMatrix = modelOffset;
				it->isBoneNode = true;
				boneID = it->id;
			}
		}
		if (boneID < 0 || boneID >= MAX_BONES) {
			LOG_WARN("Skipping bone outside the GPU bone capacity: " << boneName);
			continue;
		}
		auto weights = mesh->mBones[boneIndex]->mWeights;
		int numWeights = mesh->mBones[boneIndex]->mNumWeights;
		for (int weightIndex = 0; weightIndex < numWeights; weightIndex++) {
			unsigned int vertexID = weights[weightIndex].mVertexId;
			float weight = weights[weightIndex].mWeight;
			if (vertexID >= result.vertices.size()) {
				LOG_WARN("Skipping bone weight with invalid vertex index: " << vertexID);
				continue;
			}
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

	result.setMaterialIndex(mesh->mMaterialIndex);
	if (materials.find(mesh->mMaterialIndex) == materials.end()) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
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
		Material mat(resolveTexturePath(directory, albedoPath),
			resolveTexturePath(directory, ambientPath), resolveTexturePath(directory, specularPath),
			resolveTexturePath(directory, normalPath), resolveTexturePath(directory, shininessPath));
		materials.insert({ mesh->mMaterialIndex, std::move(mat) });
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
