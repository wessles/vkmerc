#include "VulkanUniform.h"

#include "VulkanDevice.h"

#include <vulkan/vulkan.h>

namespace vku {

	template<class UniformStructType>
	VulkanUniform<UniformStructType>::VulkanUniform(VulkanDevice* device, uint32_t numInstances) {
		this->device = device;
		this->numInstances = numInstances;

		for (size_t i = 0; i < numInstances; i++) {
			device->createBuffer(bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				buffer[i], memory[i]);
		}
	}

	template<class UniformStructType>
	VulkanUniform<UniformStructType>::~VulkanUniform() {
		for (size_t i = 0; i < numInstances; i++) {
			vkDestroyBuffer(*device, buffer[i], nullptr);
			vkFreeMemory(*device, memory[i], nullptr);
		}
	}

	template<class UniformStructType>
	void VulkanUniform<UniformStructType>::write(uint32_t i, UniformStructType& global) {
		void* data;
		vkMapMemory(*device, memory[i], 0, sizeof(UniformStructType), 0, &data);
		memcpy(data, &global, sizeof(global));
		vkUnmapMemory(*device, memory[i]);
	}

	template<class UniformStructType>
	VkWriteDescriptorSet VulkanUniform<UniformStructType>::getDescriptorWrite(uint32_t i, VkDescriptorSet& descriptorSet) {
		bufferInfo[i].buffer = buffer[i];
		bufferInfo[i].offset = 0;
		bufferInfo[i].range = sizeof(UniformStructType);

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