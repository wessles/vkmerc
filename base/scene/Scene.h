#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace vku {
	struct VulkanDevice;
	struct DescriptorLayout;
	struct VulkanDescriptorSetLayout;
	struct VulkanUniform;

	struct SceneGlobalUniform
	{
		glm::mat4 view;
		glm::mat4 proj;
		glm::vec4 camPos;
		glm::vec4 directionalLight;
		glm::vec2 screenRes;
		float time;
	};

	struct SceneInfo {
	};

	struct Scene {
		VulkanDevice* device;
		VulkanDescriptorSetLayout* globalDescriptorSetLayout;
		VkPipelineLayout globalPipelineLayout;
		std::vector<VkDescriptorSet> globalDescriptorSets;
		VulkanUniform* globalUniforms;

		Scene(VulkanDevice* device, SceneInfo info);
		~Scene();

		void updateUniforms(uint32_t i, SceneGlobalUniform* uniform);
	};
}