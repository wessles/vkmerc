#pragma once

#include <string>

#include "../scene/Object.h"
#include "../VulkanMesh.h"

namespace vku {
	struct Scene;
	struct Pass;
	struct TexturelessPbrMaterial;

	struct VulkanObjModel : Object {
		VulkanMeshBuffer* meshBuf;
		TexturelessPbrMaterial* mat;
		VulkanObjModel(const std::string& filename, Scene* scene, Pass* pass, VulkanTexture* specular_ibl, VulkanTexture* diffuse_ibl, VulkanTexture* brdf_lut);
		~VulkanObjModel();
		virtual void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial);
	};
}