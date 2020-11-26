#pragma once

#include <string>
#include <vector>

#include "../scene/Object.h"
#include "../VulkanMesh.h"
#include "../VulkanMaterial.h"

namespace vku {
	struct Scene;
	struct Pass;
	struct TexturelessPbrMaterial;

	struct VulkanObjModel : Object {
		VulkanMeshBuffer* meshBuf;
		TexturelessPbrMaterial* mat;

		glm::mat4 aabb;
		VulkanObjModel(const std::string& filename, Scene* scene, Pass* pass, std::map<std::string, std::string> macros = {});
		~VulkanObjModel();
		virtual void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) override;
		virtual glm::mat4 getAABBTransform() override;
	};
}