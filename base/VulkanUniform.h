#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vku {
	class VulkanDevice;

	template<class UniformStructType>
	class VulkanUniform {
		VulkanDevice* device;
		uint32_t numInstances;

		std::vector<VkDescriptorBufferInfo> bufferInfo;
		std::vector<VkBuffer> buffer;
		std::vector<VkDeviceMemory> memory;

	public:
		VulkanUniform(VulkanDevice* device, uint32_t numInstances);
		~VulkanUniform();

		void write(uint32_t i, UniformStructType& global);
		VkWriteDescriptorSet getDescriptorWrite(uint32_t i, VkDescriptorSet& descriptorSet);
	};
}