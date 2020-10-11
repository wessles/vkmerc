#include "VulkanUniform.h"

#include "VulkanDevice.h"

#include <vulkan/vulkan.h>

namespace vku {

	VulkanUniform::VulkanUniform(VulkanDevice* device, size_t dataSize) {
		this->device = device;
		this->dataSize = dataSize;

		device->createBuffer(dataSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			buffer, memory);
	}

	VulkanUniform::~VulkanUniform() {
		vkDestroyBuffer(*device, buffer, nullptr);
		vkFreeMemory(*device, memory, nullptr);
	}

	void VulkanUniform::write(void *global) {
		void* data;
		vkMapMemory(*device, memory, 0, dataSize, 0, &data);
		memcpy(data, global, dataSize);
		vkUnmapMemory(*device, memory);
	}
}