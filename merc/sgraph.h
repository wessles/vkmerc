#pragma once

/*
This is a basic scene graph implementation.
*/

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace vku::rgraph {
	class Scene {
		
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorSet descriptorSet;
		
	public:
		Scene() {

		}
		void process() {

		}
	};
	struct SNode {
		glm::mat4 transform;
	};
}