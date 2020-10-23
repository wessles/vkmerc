#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "../VulkanMesh.h"
#include "../VulkanMaterial.h"

namespace vku {
	struct Scene;

	struct Object {
		Scene* scene;
		Object* parent;

		glm::vec3 min = glm::vec3(std::numeric_limits<float>::infinity());
		glm::vec3 max = glm::vec3(-std::numeric_limits<float>::infinity());

		glm::mat4 localTransform = glm::mat4(1.0f);

		virtual void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) = 0;
		virtual glm::mat4 getAABBTransform() = 0;

		virtual ~Object() {}
	};
}