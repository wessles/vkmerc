#pragma once

#include <vulkan/vulkan.h>

namespace vku {
	class VulkanDevice;

	struct VulkanBufferInfo {
		VkDeviceSize size;
		VkBufferUsageFlags usage;
		VkMemoryPropertyFlags properties;
		VkBuffer buffer;
		VkDeviceMemory bufferMemory;
	};

	struct VulkanBuffer {
		VulkanBuffer(VulkanDevice *device);

	};

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		VkCommandBuffer commandBuffer = vku::cmd::beginCommandBuffer(true);

		// copy region
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vku::cmd::submitCommandBuffer(commandBuffer, vku::state::graphicsQueue);
	}
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(state::device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create vertex buffer!");
		}

		// find memory type index that is suitable
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(state::device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = vku::support::findSupportedMemoryType(memRequirements.memoryTypeBits, properties);

		// allocate the memory
		if (vkAllocateMemory(state::device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("Failed to allocate vertex buffer memory!");
		}

		// copy vertex data to newly allocated buffer
		vkBindBufferMemory(state::device, buffer, bufferMemory, 0);
	}
	template <class Type>
	void initDeviceLocalBuffer(std::vector<Type>& bufferData, VkBuffer& buffer, VkDeviceMemory& deviceMemory, VkBufferUsageFlagBits bufferUsageBit) {
		VkDeviceSize bufferSize = sizeof(bufferData[0]) * bufferData.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer,
			stagingBufferMemory);

		void* data;
		vkMapMemory(state::device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, bufferData.data(), (size_t)bufferSize);
		vkUnmapMemory(state::device, stagingBufferMemory);

		createBuffer(bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageBit,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			buffer,
			deviceMemory);

		// copy staging buffer to vertex buffer
		copyBuffer(stagingBuffer, buffer, bufferSize);

		vkDestroyBuffer(state::device, stagingBuffer, nullptr);
		vkFreeMemory(state::device, stagingBufferMemory, nullptr);
	}
}