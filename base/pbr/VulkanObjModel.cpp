#include "VulkanObjModel.h"

#include <unordered_map>
#include <filesystem>

#include <glm/glm.hpp>

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

	VulkanObjModel::VulkanObjModel(const std::string& filename, Scene* scene, Pass* pass, VulkanTexture* specular_ibl, VulkanTexture* diffuse_ibl, VulkanTexture* brdf_lut) {
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
			uniform.roughnesss = roughness;
			this->mat = new TexturelessPbrMaterial(uniform, specular_ibl, diffuse_ibl, brdf_lut, scene, pass);
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
}