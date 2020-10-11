#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vku {
	struct VulkanDevice;
	struct VulkanUniform;
	struct VulkanTexture;

	struct DescriptorLayout {
		VkDescriptorType type;
		VkShaderStageFlags stageFlags;
	};

	struct VulkanDescriptorSetLayout {
		VkDescriptorSetLayout handle;

		VulkanDevice* device;

		VulkanDescriptorSetLayout(VulkanDevice* device, std::vector<DescriptorLayout> descriptorLayouts);
		~VulkanDescriptorSetLayout();

		operator VkDescriptorSetLayout() const { return handle; }
	};

	struct VulkanDescriptorSet {
		VulkanDevice* device;

		VkDescriptorSet handle;

		VulkanDescriptorSet(VulkanDescriptorSetLayout* layout);
		~VulkanDescriptorSet();

		void write(uint32_t binding, VulkanUniform* uniform);
		void write(uint32_t binding, VulkanTexture* uniform);

		operator VkDescriptorSet() const { return handle; }
	};
}