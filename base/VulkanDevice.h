#pragma once

#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

namespace vku {
	struct VulkanContext;
	struct VulkanSwapchain;

	struct DeviceSupportInformation {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;

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
		template <class Type>
		void initDeviceLocalBuffer(std::vector<Type>& bufferData, VkBuffer& buffer, VkDeviceMemory& deviceMemory, VkBufferUsageFlagBits bufferUsageBit);

		uint32_t findSupportedMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkFormat findDepthFormat();

		// allow convenient casting
		operator VkDevice() const { return handle; }
	};
}