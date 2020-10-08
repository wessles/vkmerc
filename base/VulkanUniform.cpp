#include "VulkanUniform.h"

#include "VulkanDevice.h"

#include <vulkan/vulkan.h>

namespace vku {

	VulkanUniform::VulkanUniform(VulkanDevice* device, uint32_t numInstances, size_t dataSize) {
		this->device = device;
		this->numInstances = numInstances;
		this->dataSize = dataSize;

		this->bufferInfo.resize(numInstances);
		this->buffer.resize(numInstances);
		this->memory.resize(numInstances);

		for (size_t i = 0; i < numInstances; i++) {
			device->createBuffer(dataSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				buffer[i], memory[i]);
		}
	}

	VulkanUniform::~VulkanUniform() {
		for (size_t i = 0; i < numInstances; i++) {
			vkDestroyBuffer(*device, buffer[i], nullptr);
			vkFreeMemory(*device, memory[i], nullptr);
		}
	}

	void VulkanUniform::write(uint32_t i, void *global) {
		void* data;
		vkMapMemory(*device, memory[i], 0, dataSize, 0, &data);
		memcpy(data, global, dataSize);
		vkUnmapMemory(*device, memory[i]);
	}

	VkWriteDescriptorSet VulkanUniform::getDescriptorWrite(uint32_t i, VkDescriptorSet descriptorSet) {
		bufferInfo[i].buffer = buffer[i];
		bufferInfo[i].offset = 0;
		bufferInfo[i].range = dataSize;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo[i];
		return descriptorWrite;
	}
}