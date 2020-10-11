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

		glm::mat4 localTransform = glm::mat4(1.0f);

		virtual void render(VkCommandBuffer cmdBuf, uint32_t swapIdx, bool noMaterial) = 0;

		virtual ~Object() {}
	};
}