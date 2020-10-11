#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vku {
	struct VulkanDevice;

	struct VulkanUniform {
		VulkanDevice* device;
		size_t dataSize;

		VkBuffer buffer;
		VkDeviceMemory memory;

		VulkanUniform(VulkanDevice* device, size_t dataSize);
		~VulkanUniform();

		void write(void* global);
	};
}