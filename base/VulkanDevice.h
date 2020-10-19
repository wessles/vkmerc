#pragma once

#include <optional>
#include <vector>

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>

namespace vku {
	struct VulkanContext;
	struct VulkanSwapchain;

	struct DeviceSupportInformation {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		VkAccelerationStructureCreateInfoKHR* p;
		VkPhysicalDeviceRayTracingFeaturesKHR rtFeatures;
		VkPhysicalDeviceRayTracingPropertiesKHR rtProps;

		VkSampleCountFlags maxSampleCount;
	};

	struct DeviceSwapchainSupportInformation {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	struct VulkanDeviceInfo {
		VulkanContext* context;
		std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
			{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}
		};
		uint32_t maxDescriptorSets = 1000;
	};

	struct VulkanDevice {
		VulkanContext* context;

		DeviceSupportInformation supportInfo;
		DeviceSwapchainSupportInformation swapchainSupportInfo;

		VkPhysicalDevice physicalDevice;
		VkDevice handle;
		VkQueue graphicsQueue;
		VkQueue presentQueue;

		VkCommandPool commandPool;
		VkDescriptorPool descriptorPool;

		VulkanSwapchain* swapchain;

		VulkanDevice(VulkanDeviceInfo info);
		~VulkanDevice();

		DeviceSwapchainSupportInformation queryDeviceSwapchainSupportInformation(VkPhysicalDevice physicalDevice);
		DeviceSupportInformation queryDeviceSupportInformation(VkPhysicalDevice physicalDevice);

		VkCommandBuffer createCommandBuffer(bool oneTimeUse = false, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		VkCommandBuffer beginCommandBuffer(bool oneTimeUse = false, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		void beginCommandBuffer(VkCommandBuffer commandBuffer, bool oneTimeUse = false);
		void submitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue);

		void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

		template <typename Type>
		void initDeviceLocalBuffer(const std::vector<Type>& bufferData, VkBuffer& buffer, VkDeviceMemory& deviceMemory, VkBufferUsageFlagBits bufferUsageBit) {
			VkDeviceSize bufferSize = sizeof(bufferData[0]) * bufferData.size();

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer,
				stagingBufferMemory);

			void* data;
			vkMapMemory(*this, stagingBufferMemory, 0, bufferSize, 0, &data);
			memcpy(data, bufferData.data(), (size_t)bufferSize);
			vkUnmapMemory(*this, stagingBufferMemory);

			createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsageBit,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				buffer,
				deviceMemory);

			// copy staging buffer to vertex buffer
			copyBuffer(stagingBuffer, buffer, bufferSize);

			vkDestroyBuffer(*this, stagingBuffer, nullptr);
			vkFreeMemory(*this, stagingBufferMemory, nullptr);
		}

		uint32_t findSupportedMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkFormat findDepthFormat();

		// allow convenient casting
		operator VkDevice() const { return handle; }
	};
}