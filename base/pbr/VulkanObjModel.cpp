#include "VulkanObjModel.h"

#include <unordered_map>
#include <filesystem>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "../VulkanMesh.h"
#include "../scene/Scene.h"
#include "PbrMaterial.h"

static std::string GetBaseDir(const std::string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

namespace vku {

	VulkanObjModel::VulkanObjModel(const std::string& filename, Scene* scene, Pass* pass, std::map<std::string, std::string> macros) {
		VulkanMeshData meshData{};

		std::string base_dir = GetBaseDir(filename);
		if (base_dir.empty()) {
			base_dir = ".";
		}
#ifdef _WIN32
		base_dir += "\\";
#else
		base_dir += "/";
#endif

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str(), base_dir.c_str())) {
			throw std::runtime_error(warn + err);
		}

		if (warn.size() + err.size() > 0) {
			std::cout << "OBJ warning: " << (warn + err) << std::endl;
		}

		// load mesh data
		std::unordered_map<Vertex, uint32_t> uniqueVertices{};
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				// calculate bounding box
				min.x = std::min(min.x, vertex.pos.x);
				min.y = std::min(min.y, vertex.pos.y);
				min.z = std::min(min.z, vertex.pos.z);
				max.x = std::max(max.x, vertex.pos.x);
				max.y = std::max(max.y, vertex.pos.y);
				max.z = std::max(max.z, vertex.pos.z);

				vertex.normal = {
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2],
				};

				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.color = { 1.0f, 1.0f, 1.0f };

				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(meshData.vertices.size());
					meshData.vertices.push_back(vertex);
				}

				meshData.indices.push_back(uniqueVertices[vertex]);
			}
		}

		aabb = glm::translate(glm::mat4(1.0f), min) * glm::scale(glm::mat4(1.0f), max - min);

		this->meshBuf = new VulkanMeshBuffer(scene->device, meshData);

		// load material, based on default Blender BSDF .mtl export
		if (materials.size() > 0) {
			tinyobj::material_t mat = materials[0];

			glm::vec4 albedo = glm::vec4(static_cast<float>(mat.diffuse[0]), static_cast<float>(mat.diffuse[1]), static_cast<float>(mat.diffuse[2]), 1.0f);
			glm::vec4 emission = glm::vec4(static_cast<float>(mat.emission[0]), static_cast<float>(mat.emission[1]), static_cast<float>(mat.emission[2]), 1.0f);
			float metallic = mat.specular[0];
			float roughness = 1.0f - (static_cast<float>(mat.shininess) / 1000.0f);

			PbrUniform uniform{};
			uniform.albedo = albedo;
			uniform.emissive = emission;
			uniform.metallic = metallic;
			uniform.roughness = roughness;
			this->mat = new TexturelessPbrMaterial(uniform, scene, pass, macros);
		}
	}

	VulkanObjModel::~VulkanObjModel() {
		delete meshBuf;
		delete mat;
	}

	void VulkanObjModel::render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) {
		if (!noMaterial) {
			mat->mat->bind(cmdBuf);
			mat->matInstance->bind(cmdBuf, swapIdx);
		}
		vkCmdPushConstants(cmdBuf, mat->mat->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::mat4), &this->localTransform);
		meshBuf->draw(cmdBuf);
	}

	glm::mat4 VulkanObjModel::getAABBTransform()
	{
		return localTransform * aabb;
	}
}