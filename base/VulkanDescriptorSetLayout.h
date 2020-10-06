#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vku {
	struct VulkanDevice;

	struct DescriptorLayout {
		VkDescriptorType type;
		VkShaderStageFlags stageFlags;
	};

	struct VulkanDescriptorSetLayout {
		VkDescriptorSetLayout handle;

		VulkanDevice* device;

		VulkanDescriptorSetLayout(VulkanDevice* device, std::vector<DescriptorLayout> descriptorLayouts);
		~VulkanDescriptorSetLayout();

		VkDescriptorSet createDescriptorSet();

		operator VkDescriptorSetLayout() const { return handle; }
	};
}