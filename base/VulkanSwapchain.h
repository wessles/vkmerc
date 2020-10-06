#pragma once

#include <vector>

#include <vulkan/vulkan.h>


namespace vku {
	struct DeviceSupportInformation;
	struct VulkanDevice;
	struct VulkanImage;
	struct VulkanImageView;

	struct VulkanSwapchain {
		VulkanDevice* device;

		VkSwapchainKHR swapchain;

		VkFormat screenFormat;
		VkFormat depthFormat;
		VkExtent2D swapChainExtent;

		uint32_t swapChainLength;
		std::vector<VkImage> swapChainImages;
		std::vector<VulkanImageView*> swapChainImageViews;

		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;
		std::vector<VkFence> imagesInFlight;

		VulkanSwapchain(VulkanDevice* device);
		~VulkanSwapchain();

		void createSwapchain();
		void destroySwapchain();

		// allow convenient casting
		operator VkSwapchainKHR() const { return swapchain; }

	private:
		void createSyncObjects();
		void destroySyncObjects();
	};
}