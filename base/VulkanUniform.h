#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vku {
	class VulkanDevice;

	class VulkanUniform {
		VulkanDevice* device;
		uint32_t numInstances;
		size_t dataSize;

		std::vector<VkDescriptorBufferInfo> bufferInfo;
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;

	public:
		VulkanUniform(VulkanDevice* device, uint32_t numInstances, size_t dataSize);
		~VulkanUniform();

		void write(uint32_t i, void* global);
		VkWriteDescriptorSet getDescriptorWrite(uint32_t i, VkDescriptorSet descriptorSet);
	};

	/*template<class UniformStructType>
	class UniformBuffer {
		std::vector<VkDescriptorBufferInfo> bufferInfo;
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;

	public:
		void create(VulkanDevice* device) {
			for (size_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
				vku::buffer::createBuffer(bufferSize,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					buffer[i], memory[i]);
			}
		}
		void write(uint32_t i, UniformStructType& global) {
			void* data;
			vkMapMemory(vku::state::device, memory[i], 0, sizeof(UniformStructType), 0, &data);
			memcpy(data, &global, sizeof(global));
			vkUnmapMemory(vku::state::device, memory[i]);
		}
		void destroy() {
			for (size_t i = 0; i < vku::state::SWAPCHAIN_SIZE; i++) {
				vkDestroyBuffer(vku::state::device, buffer[i], nullptr);
				vkFreeMemory(vku::state::device, memory[i], nullptr);
			}
		}
		VkWriteDescriptorSet getDescriptorWrite(uint32_t i, VkDescriptorSet& descriptorSet) {
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
	};*/
}