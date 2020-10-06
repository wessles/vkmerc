#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vku {
	struct VulkanDevice;
	struct DescriptorLayout;
	struct VulkanDescriptorSetLayout;

	struct SceneInfo {
		std::vector<DescriptorLayout> globalDescriptors;
	};

	struct Scene {
		VulkanDevice* device;
		VulkanDescriptorSetLayout* globalDescriptorSetLayout;
		VkPipelineLayout globalPipelineLayout;
		std::vector<VkDescriptorSet> globalDescriptorSets;

		Scene(VulkanDevice* device, SceneInfo info);
		~Scene();
	};
}